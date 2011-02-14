//===- ConstantFold.cpp - LLVM constant folder ----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements folding of constants for LLVM.  This implements the
// (internal) ConstantFold.h interface, which is used by the
// ConstantExpr::get* methods to automatically fold constants when possible.
//
// The current constant folding implementation is implemented in two pieces: the
// pieces that don't need TargetData, and the pieces that do. This is to avoid
// a dependence in VMCore on Target.
//
//===----------------------------------------------------------------------===//

#include "ConstantFold.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/GlobalAlias.h"
#include "llvm/GlobalVariable.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MathExtras.h"
#include <limits>
using namespace llvm;

//===----------------------------------------------------------------------===//
//                ConstantFold*Instruction Implementations
//===----------------------------------------------------------------------===//

/// BitCastConstantVector - Convert the specified ConstantVector node to the
/// specified vector type.  At this point, we know that the elements of the
/// input vector constant are all simple integer or FP values.
static Constant *BitCastConstantVector(ConstantVector *CV,
                                       const VectorType *DstTy) {

  if (CV->isAllOnesValue() && DstTy->getElementType()->isIntegerTy())
      return Constant::getAllOnesValue(DstTy);
  if (CV->isNullValue()) return Constant::getNullValue(DstTy);

  // If this cast changes element count then we can't handle it here:
  // doing so requires endianness information.  This should be handled by
  // Analysis/ConstantFolding.cpp
  unsigned NumElts = DstTy->getNumElements();
  if (NumElts != CV->getNumOperands())
    return 0;

  // Check to verify that all elements of the input are simple.
  for (unsigned i = 0; i != NumElts; ++i) {
    if (!isa<ConstantInt>(CV->getOperand(i)) &&
        !isa<ConstantFP>(CV->getOperand(i)))
      return 0;
  }

  // Bitcast each element now.
  std::vector<Constant*> Result;
  const Type *DstEltTy = DstTy->getElementType();
  for (unsigned i = 0; i != NumElts; ++i)
    Result.push_back(ConstantExpr::getBitCast(CV->getOperand(i),
                                                    DstEltTy));
  return ConstantVector::get(Result);
}

/// This function determines which opcode to use to fold two constant cast 
/// expressions together. It uses CastInst::isEliminableCastPair to determine
/// the opcode. Consequently its just a wrapper around that function.
/// @brief Determine if it is valid to fold a cast of a cast
static unsigned
foldConstantCastPair(
  unsigned opc,          ///< opcode of the second cast constant expression
  ConstantExpr *Op,      ///< the first cast constant expression
  const Type *DstTy      ///< desintation type of the first cast
) {
  assert(Op && Op->isCast() && "Can't fold cast of cast without a cast!");
  assert(DstTy && DstTy->isFirstClassType() && "Invalid cast destination type");
  assert(CastInst::isCast(opc) && "Invalid cast opcode");

  // The the types and opcodes for the two Cast constant expressions
  const Type *SrcTy = Op->getOperand(0)->getType();
  const Type *MidTy = Op->getType();
  Instruction::CastOps firstOp = Instruction::CastOps(Op->getOpcode());
  Instruction::CastOps secondOp = Instruction::CastOps(opc);

  // Let CastInst::isEliminableCastPair do the heavy lifting.
  return CastInst::isEliminableCastPair(firstOp, secondOp, SrcTy, MidTy, DstTy,
                                        Type::getInt64Ty(DstTy->getContext()));
}

static Constant *FoldBitCast(Constant *V, const Type *DestTy) {
  const Type *SrcTy = V->getType();
  if (SrcTy == DestTy)
    return V; // no-op cast

  // Check to see if we are casting a pointer to an aggregate to a pointer to
  // the first element.  If so, return the appropriate GEP instruction.
  if (const PointerType *PTy = dyn_cast<PointerType>(V->getType()))
    if (const PointerType *DPTy = dyn_cast<PointerType>(DestTy))
      if (PTy->getAddressSpace() == DPTy->getAddressSpace()) {
        SmallVector<Value*, 8> IdxList;
        Value *Zero =
          Constant::getNullValue(Type::getInt32Ty(DPTy->getContext()));
        IdxList.push_back(Zero);
        const Type *ElTy = PTy->getElementType();
        while (ElTy != DPTy->getElementType()) {
          if (const StructType *STy = dyn_cast<StructType>(ElTy)) {
            if (STy->getNumElements() == 0) break;
            ElTy = STy->getElementType(0);
            IdxList.push_back(Zero);
          } else if (const SequentialType *STy = 
                     dyn_cast<SequentialType>(ElTy)) {
            if (ElTy->isPointerTy()) break;  // Can't index into pointers!
            ElTy = STy->getElementType();
            IdxList.push_back(Zero);
          } else {
            break;
          }
        }

        if (ElTy == DPTy->getElementType())
          // This GEP is inbounds because all indices are zero.
          return ConstantExpr::getInBoundsGetElementPtr(V, &IdxList[0],
                                                        IdxList.size());
      }

  // Handle casts from one vector constant to another.  We know that the src 
  // and dest type have the same size (otherwise its an illegal cast).
  if (const VectorType *DestPTy = dyn_cast<VectorType>(DestTy)) {
    if (const VectorType *SrcTy = dyn_cast<VectorType>(V->getType())) {
      assert(DestPTy->getBitWidth() == SrcTy->getBitWidth() &&
             "Not cast between same sized vectors!");
      SrcTy = NULL;
      // First, check for null.  Undef is already handled.
      if (isa<ConstantAggregateZero>(V))
        return Constant::getNullValue(DestTy);

      if (ConstantVector *CV = dyn_cast<ConstantVector>(V))
        return BitCastConstantVector(CV, DestPTy);
    }

    // Canonicalize scalar-to-vector bitcasts into vector-to-vector bitcasts
    // This allows for other simplifications (although some of them
    // can only be handled by Analysis/ConstantFolding.cpp).
    if (isa<ConstantInt>(V) || isa<ConstantFP>(V))
      return ConstantExpr::getBitCast(ConstantVector::get(&V, 1), DestPTy);
  }

  // Finally, implement bitcast folding now.   The code below doesn't handle
  // bitcast right.
  if (isa<ConstantPointerNull>(V))  // ptr->ptr cast.
    return ConstantPointerNull::get(cast<PointerType>(DestTy));

  // Handle integral constant input.
  if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
    if (DestTy->isIntegerTy())
      // Integral -> Integral. This is a no-op because the bit widths must
      // be the same. Consequently, we just fold to V.
      return V;

    if (DestTy->isFloatingPointTy())
      return ConstantFP::get(DestTy->getContext(),
                             APFloat(CI->getValue(),
                                     !DestTy->isPPC_FP128Ty()));

    // Otherwise, can't fold this (vector?)
    return 0;
  }

  // Handle ConstantFP input: FP -> Integral.
  if (ConstantFP *FP = dyn_cast<ConstantFP>(V))
    return ConstantInt::get(FP->getContext(),
                            FP->getValueAPF().bitcastToAPInt());

  return 0;
}


/// ExtractConstantBytes - V is an integer constant which only has a subset of
/// its bytes used.  The bytes used are indicated by ByteStart (which is the
/// first byte used, counting from the least significant byte) and ByteSize,
/// which is the number of bytes used.
///
/// This function analyzes the specified constant to see if the specified byte
/// range can be returned as a simplified constant.  If so, the constant is
/// returned, otherwise null is returned.
/// 
static Constant *ExtractConstantBytes(Constant *C, unsigned ByteStart,
                                      unsigned ByteSize) {
  assert(C->getType()->isIntegerTy() &&
         (cast<IntegerType>(C->getType())->getBitWidth() & 7) == 0 &&
         "Non-byte sized integer input");
  unsigned CSize = cast<IntegerType>(C->getType())->getBitWidth()/8;
  assert(ByteSize && "Must be accessing some piece");
  assert(ByteStart+ByteSize <= CSize && "Extracting invalid piece from input");
  assert(ByteSize != CSize && "Should not extract everything");
  
  // Constant Integers are simple.
  if (ConstantInt *CI = dyn_cast<ConstantInt>(C)) {
    APInt V = CI->getValue();
    if (ByteStart)
      V = V.lshr(ByteStart*8);
    V = V.trunc(ByteSize*8);
    return ConstantInt::get(CI->getContext(), V);
  }
  
  // In the input is a constant expr, we might be able to recursively simplify.
  // If not, we definitely can't do anything.
  ConstantExpr *CE = dyn_cast<ConstantExpr>(C);
  if (CE == 0) return 0;
  
  switch (CE->getOpcode()) {
  default: return 0;
  case Instruction::Or: {
    Constant *RHS = ExtractConstantBytes(CE->getOperand(1), ByteStart,ByteSize);
    if (RHS == 0)
      return 0;
    
    // X | -1 -> -1.
    if (ConstantInt *RHSC = dyn_cast<ConstantInt>(RHS))
      if (RHSC->isAllOnesValue())
        return RHSC;
    
    Constant *LHS = ExtractConstantBytes(CE->getOperand(0), ByteStart,ByteSize);
    if (LHS == 0)
      return 0;
    return ConstantExpr::getOr(LHS, RHS);
  }
  case Instruction::And: {
    Constant *RHS = ExtractConstantBytes(CE->getOperand(1), ByteStart,ByteSize);
    if (RHS == 0)
      return 0;
    
    // X & 0 -> 0.
    if (RHS->isNullValue())
      return RHS;
    
    Constant *LHS = ExtractConstantBytes(CE->getOperand(0), ByteStart,ByteSize);
    if (LHS == 0)
      return 0;
    return ConstantExpr::getAnd(LHS, RHS);
  }
  case Instruction::LShr: {
    ConstantInt *Amt = dyn_cast<ConstantInt>(CE->getOperand(1));
    if (Amt == 0)
      return 0;
    unsigned ShAmt = Amt->getZExtValue();
    // Cannot analyze non-byte shifts.
    if ((ShAmt & 7) != 0)
      return 0;
    ShAmt >>= 3;
    
    // If the extract is known to be all zeros, return zero.
    if (ByteStart >= CSize-ShAmt)
      return Constant::getNullValue(IntegerType::get(CE->getContext(),
                                                     ByteSize*8));
    // If the extract is known to be fully in the input, extract it.
    if (ByteStart+ByteSize+ShAmt <= CSize)
      return ExtractConstantBytes(CE->getOperand(0), ByteStart+ShAmt, ByteSize);
    
    // TODO: Handle the 'partially zero' case.
    return 0;
  }
    
  case Instruction::Shl: {
    ConstantInt *Amt = dyn_cast<ConstantInt>(CE->getOperand(1));
    if (Amt == 0)
      return 0;
    unsigned ShAmt = Amt->getZExtValue();
    // Cannot analyze non-byte shifts.
    if ((ShAmt & 7) != 0)
      return 0;
    ShAmt >>= 3;
    
    // If the extract is known to be all zeros, return zero.
    if (ByteStart+ByteSize <= ShAmt)
      return Constant::getNullValue(IntegerType::get(CE->getContext(),
                                                     ByteSize*8));
    // If the extract is known to be fully in the input, extract it.
    if (ByteStart >= ShAmt)
      return ExtractConstantBytes(CE->getOperand(0), ByteStart-ShAmt, ByteSize);
    
    // TODO: Handle the 'partially zero' case.
    return 0;
  }
      
  case Instruction::ZExt: {
    unsigned SrcBitSize =
      cast<IntegerType>(CE->getOperand(0)->getType())->getBitWidth();
    
    // If extracting something that is completely zero, return 0.
    if (ByteStart*8 >= SrcBitSize)
      return Constant::getNullValue(IntegerType::get(CE->getContext(),
                                                     ByteSize*8));

    // If exactly extracting the input, return it.
    if (ByteStart == 0 && ByteSize*8 == SrcBitSize)
      return CE->getOperand(0);
    
    // If extracting something completely in the input, if if the input is a
    // multiple of 8 bits, recurse.
    if ((SrcBitSize&7) == 0 && (ByteStart+ByteSize)*8 <= SrcBitSize)
      return ExtractConstantBytes(CE->getOperand(0), ByteStart, ByteSize);
      
    // Otherwise, if extracting a subset of the input, which is not multiple of
    // 8 bits, do a shift and trunc to get the bits.
    if ((ByteStart+ByteSize)*8 < SrcBitSize) {
      assert((SrcBitSize&7) && "Shouldn't get byte sized case here");
      Constant *Res = CE->getOperand(0);
      if (ByteStart)
        Res = ConstantExpr::getLShr(Res, 
                                 ConstantInt::get(Res->getType(), ByteStart*8));
      return ConstantExpr::getTrunc(Res, IntegerType::get(C->getContext(),
                                                          ByteSize*8));
    }
    
    // TODO: Handle the 'partially zero' case.
    return 0;
  }
  }
}

/// getFoldedSizeOf - Return a ConstantExpr with type DestTy for sizeof
/// on Ty, with any known factors factored out. If Folded is false,
/// return null if no factoring was possible, to avoid endlessly
/// bouncing an unfoldable expression back into the top-level folder.
///
static Constant *getFoldedSizeOf(const Type *Ty, const Type *DestTy,
                                 bool Folded) {
  if (const ArrayType *ATy = dyn_cast<ArrayType>(Ty)) {
    Constant *N = ConstantInt::get(DestTy, ATy->getNumElements());
    Constant *E = getFoldedSizeOf(ATy->getElementType(), DestTy, true);
    return ConstantExpr::getNUWMul(E, N);
  }

  if (const StructType *STy = dyn_cast<StructType>(Ty))
    if (!STy->isPacked()) {
      unsigned NumElems = STy->getNumElements();
      // An empty struct has size zero.
      if (NumElems == 0)
        return ConstantExpr::getNullValue(DestTy);
      // Check for a struct with all members having the same size.
      Constant *MemberSize =
        getFoldedSizeOf(STy->getElementType(0), DestTy, true);
      bool AllSame = true;
      for (unsigned i = 1; i != NumElems; ++i)
        if (MemberSize !=
            getFoldedSizeOf(STy->getElementType(i), DestTy, true)) {
          AllSame = false;
          break;
        }
      if (AllSame) {
        Constant *N = ConstantInt::get(DestTy, NumElems);
        return ConstantExpr::getNUWMul(MemberSize, N);
      }
    }

  // Pointer size doesn't depend on the pointee type, so canonicalize them
  // to an arbitrary pointee.
  if (const PointerType *PTy = dyn_cast<PointerType>(Ty))
    if (!PTy->getElementType()->isIntegerTy(1))
      return
        getFoldedSizeOf(PointerType::get(IntegerType::get(PTy->getContext(), 1),
                                         PTy->getAddressSpace()),
                        DestTy, true);

  // If there's no interesting folding happening, bail so that we don't create
  // a constant that looks like it needs folding but really doesn't.
  if (!Folded)
    return 0;

  // Base case: Get a regular sizeof expression.
  Constant *C = ConstantExpr::getSizeOf(Ty);
  C = ConstantExpr::getCast(CastInst::getCastOpcode(C, false,
                                                    DestTy, false),
                            C, DestTy);
  return C;
}

/// getFoldedAlignOf - Return a ConstantExpr with type DestTy for alignof
/// on Ty, with any known factors factored out. If Folded is false,
/// return null if no factoring was possible, to avoid endlessly
/// bouncing an unfoldable expression back into the top-level folder.
///
static Constant *getFoldedAlignOf(const Type *Ty, const Type *DestTy,
                                  bool Folded) {
  // The alignment of an array is equal to the alignment of the
  // array element. Note that this is not always true for vectors.
  if (const ArrayType *ATy = dyn_cast<ArrayType>(Ty)) {
    Constant *C = ConstantExpr::getAlignOf(ATy->getElementType());
    C = ConstantExpr::getCast(CastInst::getCastOpcode(C, false,
                                                      DestTy,
                                                      false),
                              C, DestTy);
    return C;
  }

  if (const StructType *STy = dyn_cast<StructType>(Ty)) {
    // Packed structs always have an alignment of 1.
    if (STy->isPacked())
      return ConstantInt::get(DestTy, 1);

    // Otherwise, struct alignment is the maximum alignment of any member.
    // Without target data, we can't compare much, but we can check to see
    // if all the members have the same alignment.
    unsigned NumElems = STy->getNumElements();
    // An empty struct has minimal alignment.
    if (NumElems == 0)
      return ConstantInt::get(DestTy, 1);
    // Check for a struct with all members having the same alignment.
    Constant *MemberAlign =
      getFoldedAlignOf(STy->getElementType(0), DestTy, true);
    bool AllSame = true;
    for (unsigned i = 1; i != NumElems; ++i)
      if (MemberAlign != getFoldedAlignOf(STy->getElementType(i), DestTy, true)) {
        AllSame = false;
        break;
      }
    if (AllSame)
      return MemberAlign;
  }

  // Pointer alignment doesn't depend on the pointee type, so canonicalize them
  // to an arbitrary pointee.
  if (const PointerType *PTy = dyn_cast<PointerType>(Ty))
    if (!PTy->getElementType()->isIntegerTy(1))
      return
        getFoldedAlignOf(PointerType::get(IntegerType::get(PTy->getContext(),
                                                           1),
                                          PTy->getAddressSpace()),
                         DestTy, true);

  // If there's no interesting folding happening, bail so that we don't create
  // a constant that looks like it needs folding but really doesn't.
  if (!Folded)
    return 0;

  // Base case: Get a regular alignof expression.
  Constant *C = ConstantExpr::getAlignOf(Ty);
  C = ConstantExpr::getCast(CastInst::getCastOpcode(C, false,
                                                    DestTy, false),
                            C, DestTy);
  return C;
}

/// getFoldedOffsetOf - Return a ConstantExpr with type DestTy for offsetof
/// on Ty and FieldNo, with any known factors factored out. If Folded is false,
/// return null if no factoring was possible, to avoid endlessly
/// bouncing an unfoldable expression back into the top-level folder.
///
static Constant *getFoldedOffsetOf(const Type *Ty, Constant *FieldNo,
                                   const Type *DestTy,
                                   bool Folded) {
  if (const ArrayType *ATy = dyn_cast<ArrayType>(Ty)) {
    Constant *N = ConstantExpr::getCast(CastInst::getCastOpcode(FieldNo, false,
                                                                DestTy, false),
                                        FieldNo, DestTy);
    Constant *E = getFoldedSizeOf(ATy->getElementType(), DestTy, true);
    return ConstantExpr::getNUWMul(E, N);
  }

  if (const StructType *STy = dyn_cast<StructType>(Ty))
    if (!STy->isPacked()) {
      unsigned NumElems = STy->getNumElements();
      // An empty struct has no members.
      if (NumElems == 0)
        return 0;
      // Check for a struct with all members having the same size.
      Constant *MemberSize =
        getFoldedSizeOf(STy->getElementType(0), DestTy, true);
      bool AllSame = true;
      for (unsigned i = 1; i != NumElems; ++i)
        if (MemberSize !=
            getFoldedSizeOf(STy->getElementType(i), DestTy, true)) {
          AllSame = false;
          break;
        }
      if (AllSame) {
        Constant *N = ConstantExpr::getCast(CastInst::getCastOpcode(FieldNo,
                                                                    false,
                                                                    DestTy,
                                                                    false),
                                            FieldNo, DestTy);
        return ConstantExpr::getNUWMul(MemberSize, N);
      }
    }

  // If there's no interesting folding happening, bail so that we don't create
  // a constant that looks like it needs folding but really doesn't.
  if (!Folded)
    return 0;

  // Base case: Get a regular offsetof expression.
  Constant *C = ConstantExpr::getOffsetOf(Ty, FieldNo);
  C = ConstantExpr::getCast(CastInst::getCastOpcode(C, false,
                                                    DestTy, false),
                            C, DestTy);
  return C;
}

Constant *llvm::ConstantFoldCastInstruction(unsigned opc, Constant *V,
                                            const Type *DestTy) {
  if (isa<UndefValue>(V)) {
    // zext(undef) = 0, because the top bits will be zero.
    // sext(undef) = 0, because the top bits will all be the same.
    // [us]itofp(undef) = 0, because the result value is bounded.
    if (opc == Instruction::ZExt || opc == Instruction::SExt ||
        opc == Instruction::UIToFP || opc == Instruction::SIToFP)
      return Constant::getNullValue(DestTy);
    return UndefValue::get(DestTy);
  }

  // No compile-time operations on this type yet.
  if (V->getType()->isPPC_FP128Ty() || DestTy->isPPC_FP128Ty())
    return 0;

  if (V->isNullValue() && !DestTy->isX86_MMXTy())
    return Constant::getNullValue(DestTy);

  // If the cast operand is a constant expression, there's a few things we can
  // do to try to simplify it.
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(V)) {
    if (CE->isCast()) {
      // Try hard to fold cast of cast because they are often eliminable.
      if (unsigned newOpc = foldConstantCastPair(opc, CE, DestTy))
        return ConstantExpr::getCast(newOpc, CE->getOperand(0), DestTy);
    } else if (CE->getOpcode() == Instruction::GetElementPtr) {
      // If all of the indexes in the GEP are null values, there is no pointer
      // adjustment going on.  We might as well cast the source pointer.
      bool isAllNull = true;
      for (unsigned i = 1, e = CE->getNumOperands(); i != e; ++i)
        if (!CE->getOperand(i)->isNullValue()) {
          isAllNull = false;
          break;
        }
      if (isAllNull)
        // This is casting one pointer type to another, always BitCast
        return ConstantExpr::getPointerCast(CE->getOperand(0), DestTy);
    }
  }

  // If the cast operand is a constant vector, perform the cast by
  // operating on each element. In the cast of bitcasts, the element
  // count may be mismatched; don't attempt to handle that here.
  if (ConstantVector *CV = dyn_cast<ConstantVector>(V))
    if (DestTy->isVectorTy() &&
        cast<VectorType>(DestTy)->getNumElements() ==
        CV->getType()->getNumElements()) {
      std::vector<Constant*> res;
      const VectorType *DestVecTy = cast<VectorType>(DestTy);
      const Type *DstEltTy = DestVecTy->getElementType();
      for (unsigned i = 0, e = CV->getType()->getNumElements(); i != e; ++i)
        res.push_back(ConstantExpr::getCast(opc,
                                            CV->getOperand(i), DstEltTy));
      return ConstantVector::get(DestVecTy, res);
    }

  // We actually have to do a cast now. Perform the cast according to the
  // opcode specified.
  switch (opc) {
  default:
    llvm_unreachable("Failed to cast constant expression");
  case Instruction::FPTrunc:
  case Instruction::FPExt:
    if (ConstantFP *FPC = dyn_cast<ConstantFP>(V)) {
      bool ignored;
      APFloat Val = FPC->getValueAPF();
      Val.convert(DestTy->isFloatTy() ? APFloat::IEEEsingle :
                  DestTy->isDoubleTy() ? APFloat::IEEEdouble :
                  DestTy->isX86_FP80Ty() ? APFloat::x87DoubleExtended :
                  DestTy->isFP128Ty() ? APFloat::IEEEquad :
                  APFloat::Bogus,
                  APFloat::rmNearestTiesToEven, &ignored);
      return ConstantFP::get(V->getContext(), Val);
    }
    return 0; // Can't fold.
  case Instruction::FPToUI: 
  case Instruction::FPToSI:
    if (ConstantFP *FPC = dyn_cast<ConstantFP>(V)) {
      const APFloat &V = FPC->getValueAPF();
      bool ignored;
      uint64_t x[2]; 
      uint32_t DestBitWidth = cast<IntegerType>(DestTy)->getBitWidth();
      (void) V.convertToInteger(x, DestBitWidth, opc==Instruction::FPToSI,
                                APFloat::rmTowardZero, &ignored);
      APInt Val(DestBitWidth, 2, x);
      return ConstantInt::get(FPC->getContext(), Val);
    }
    return 0; // Can't fold.
  case Instruction::IntToPtr:   //always treated as unsigned
    if (V->isNullValue())       // Is it an integral null value?
      return ConstantPointerNull::get(cast<PointerType>(DestTy));
    return 0;                   // Other pointer types cannot be casted
  case Instruction::PtrToInt:   // always treated as unsigned
    // Is it a null pointer value?
    if (V->isNullValue())
      return ConstantInt::get(DestTy, 0);
    // If this is a sizeof-like expression, pull out multiplications by
    // known factors to expose them to subsequent folding. If it's an
    // alignof-like expression, factor out known factors.
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(V))
      if (CE->getOpcode() == Instruction::GetElementPtr &&
          CE->getOperand(0)->isNullValue()) {
        const Type *Ty =
          cast<PointerType>(CE->getOperand(0)->getType())->getElementType();
        if (CE->getNumOperands() == 2) {
          // Handle a sizeof-like expression.
          Constant *Idx = CE->getOperand(1);
          bool isOne = isa<ConstantInt>(Idx) && cast<ConstantInt>(Idx)->isOne();
          if (Constant *C = getFoldedSizeOf(Ty, DestTy, !isOne)) {
            Idx = ConstantExpr::getCast(CastInst::getCastOpcode(Idx, true,
                                                                DestTy, false),
                                        Idx, DestTy);
            return ConstantExpr::getMul(C, Idx);
          }
        } else if (CE->getNumOperands() == 3 &&
                   CE->getOperand(1)->isNullValue()) {
          // Handle an alignof-like expression.
          if (const StructType *STy = dyn_cast<StructType>(Ty))
            if (!STy->isPacked()) {
              ConstantInt *CI = cast<ConstantInt>(CE->getOperand(2));
              if (CI->isOne() &&
                  STy->getNumElements() == 2 &&
                  STy->getElementType(0)->isIntegerTy(1)) {
                return getFoldedAlignOf(STy->getElementType(1), DestTy, false);
              }
            }
          // Handle an offsetof-like expression.
          if (Ty->isStructTy() || Ty->isArrayTy()) {
            if (Constant *C = getFoldedOffsetOf(Ty, CE->getOperand(2),
                                                DestTy, false))
              return C;
          }
        }
      }
    // Other pointer types cannot be casted
    return 0;
  case Instruction::UIToFP:
  case Instruction::SIToFP:
    if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
      APInt api = CI->getValue();
      APFloat apf(APInt::getNullValue(DestTy->getPrimitiveSizeInBits()), true);
      (void)apf.convertFromAPInt(api, 
                                 opc==Instruction::SIToFP,
                                 APFloat::rmNearestTiesToEven);
      return ConstantFP::get(V->getContext(), apf);
    }
    return 0;
  case Instruction::ZExt:
    if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
      uint32_t BitWidth = cast<IntegerType>(DestTy)->getBitWidth();
      return ConstantInt::get(V->getContext(),
                              CI->getValue().zext(BitWidth));
    }
    return 0;
  case Instruction::SExt:
    if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
      uint32_t BitWidth = cast<IntegerType>(DestTy)->getBitWidth();
      return ConstantInt::get(V->getContext(),
                              CI->getValue().sext(BitWidth));
    }
    return 0;
  case Instruction::Trunc: {
    uint32_t DestBitWidth = cast<IntegerType>(DestTy)->getBitWidth();
    if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
      return ConstantInt::get(V->getContext(),
                              CI->getValue().trunc(DestBitWidth));
    }
    
    // The input must be a constantexpr.  See if we can simplify this based on
    // the bytes we are demanding.  Only do this if the source and dest are an
    // even multiple of a byte.
    if ((DestBitWidth & 7) == 0 &&
        (cast<IntegerType>(V->getType())->getBitWidth() & 7) == 0)
      if (Constant *Res = ExtractConstantBytes(V, 0, DestBitWidth / 8))
        return Res;
      
    return 0;
  }
  case Instruction::BitCast:
    return FoldBitCast(V, DestTy);
  }
}

Constant *llvm::ConstantFoldSelectInstruction(Constant *Cond,
                                              Constant *V1, Constant *V2) {
  if (ConstantInt *CB = dyn_cast<ConstantInt>(Cond))
    return CB->getZExtValue() ? V1 : V2;

  // Check for zero aggregate and ConstantVector of zeros
  if (Cond->isNullValue()) return V2;

  if (ConstantVector* CondV = dyn_cast<ConstantVector>(Cond)) {

    if (CondV->isAllOnesValue()) return V1;

    const VectorType *VTy = cast<VectorType>(V1->getType());
    ConstantVector *CP1 = dyn_cast<ConstantVector>(V1);
    ConstantVector *CP2 = dyn_cast<ConstantVector>(V2);

    if ((CP1 || isa<ConstantAggregateZero>(V1)) &&
        (CP2 || isa<ConstantAggregateZero>(V2))) {

      // Find the element type of the returned vector
      const Type *EltTy = VTy->getElementType();
      unsigned NumElem = VTy->getNumElements();
      std::vector<Constant*> Res(NumElem);

      bool Valid = true;
      for (unsigned i = 0; i < NumElem; ++i) {
        ConstantInt* c = dyn_cast<ConstantInt>(CondV->getOperand(i));
        if (!c) {
          Valid = false;
          break;
        }
        Constant *C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
        Constant *C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
        Res[i] = c->getZExtValue() ? C1 : C2;
      }
      // If we were able to build the vector, return it
      if (Valid) return ConstantVector::get(Res);
    }
  }


  if (isa<UndefValue>(V1)) return V2;
  if (isa<UndefValue>(V2)) return V1;
  if (isa<UndefValue>(Cond)) return V1;
  if (V1 == V2) return V1;

  if (ConstantExpr *TrueVal = dyn_cast<ConstantExpr>(V1)) {
    if (TrueVal->getOpcode() == Instruction::Select)
      if (TrueVal->getOperand(0) == Cond)
	return ConstantExpr::getSelect(Cond, TrueVal->getOperand(1), V2);
  }
  if (ConstantExpr *FalseVal = dyn_cast<ConstantExpr>(V2)) {
    if (FalseVal->getOpcode() == Instruction::Select)
      if (FalseVal->getOperand(0) == Cond)
	return ConstantExpr::getSelect(Cond, V1, FalseVal->getOperand(2));
  }

  return 0;
}

Constant *llvm::ConstantFoldExtractElementInstruction(Constant *Val,
                                                      Constant *Idx) {
  if (isa<UndefValue>(Val))  // ee(undef, x) -> undef
    return UndefValue::get(cast<VectorType>(Val->getType())->getElementType());
  if (Val->isNullValue())  // ee(zero, x) -> zero
    return Constant::getNullValue(
                          cast<VectorType>(Val->getType())->getElementType());

  if (ConstantVector *CVal = dyn_cast<ConstantVector>(Val)) {
    if (ConstantInt *CIdx = dyn_cast<ConstantInt>(Idx)) {
      return CVal->getOperand(CIdx->getZExtValue());
    } else if (isa<UndefValue>(Idx)) {
      // ee({w,x,y,z}, undef) -> w (an arbitrary value).
      return CVal->getOperand(0);
    }
  }
  return 0;
}

Constant *llvm::ConstantFoldInsertElementInstruction(Constant *Val,
                                                     Constant *Elt,
                                                     Constant *Idx) {
  ConstantInt *CIdx = dyn_cast<ConstantInt>(Idx);
  if (!CIdx) return 0;
  APInt idxVal = CIdx->getValue();
  if (isa<UndefValue>(Val)) { 
    // Insertion of scalar constant into vector undef
    // Optimize away insertion of undef
    if (isa<UndefValue>(Elt))
      return Val;
    // Otherwise break the aggregate undef into multiple undefs and do
    // the insertion
    unsigned numOps = 
      cast<VectorType>(Val->getType())->getNumElements();
    std::vector<Constant*> Ops; 
    Ops.reserve(numOps);
    for (unsigned i = 0; i < numOps; ++i) {
      Constant *Op =
        (idxVal == i) ? Elt : UndefValue::get(Elt->getType());
      Ops.push_back(Op);
    }
    return ConstantVector::get(Ops);
  }
  if (isa<ConstantAggregateZero>(Val)) {
    // Insertion of scalar constant into vector aggregate zero
    // Optimize away insertion of zero
    if (Elt->isNullValue())
      return Val;
    // Otherwise break the aggregate zero into multiple zeros and do
    // the insertion
    unsigned numOps = 
      cast<VectorType>(Val->getType())->getNumElements();
    std::vector<Constant*> Ops; 
    Ops.reserve(numOps);
    for (unsigned i = 0; i < numOps; ++i) {
      Constant *Op =
        (idxVal == i) ? Elt : Constant::getNullValue(Elt->getType());
      Ops.push_back(Op);
    }
    return ConstantVector::get(Ops);
  }
  if (ConstantVector *CVal = dyn_cast<ConstantVector>(Val)) {
    // Insertion of scalar constant into vector constant
    std::vector<Constant*> Ops; 
    Ops.reserve(CVal->getNumOperands());
    for (unsigned i = 0; i < CVal->getNumOperands(); ++i) {
      Constant *Op =
        (idxVal == i) ? Elt : cast<Constant>(CVal->getOperand(i));
      Ops.push_back(Op);
    }
    return ConstantVector::get(Ops);
  }

  return 0;
}

/// GetVectorElement - If C is a ConstantVector, ConstantAggregateZero or Undef
/// return the specified element value.  Otherwise return null.
static Constant *GetVectorElement(Constant *C, unsigned EltNo) {
  if (ConstantVector *CV = dyn_cast<ConstantVector>(C))
    return CV->getOperand(EltNo);

  const Type *EltTy = cast<VectorType>(C->getType())->getElementType();
  if (isa<ConstantAggregateZero>(C))
    return Constant::getNullValue(EltTy);
  if (isa<UndefValue>(C))
    return UndefValue::get(EltTy);
  return 0;
}

Constant *llvm::ConstantFoldShuffleVectorInstruction(Constant *V1,
                                                     Constant *V2,
                                                     Constant *Mask) {
  // Undefined shuffle mask -> undefined value.
  if (isa<UndefValue>(Mask)) return UndefValue::get(V1->getType());

  unsigned MaskNumElts = cast<VectorType>(Mask->getType())->getNumElements();
  unsigned SrcNumElts = cast<VectorType>(V1->getType())->getNumElements();
  const Type *EltTy = cast<VectorType>(V1->getType())->getElementType();

  // Loop over the shuffle mask, evaluating each element.
  SmallVector<Constant*, 32> Result;
  for (unsigned i = 0; i != MaskNumElts; ++i) {
    Constant *InElt = GetVectorElement(Mask, i);
    if (InElt == 0) return 0;

    if (isa<UndefValue>(InElt))
      InElt = UndefValue::get(EltTy);
    else if (ConstantInt *CI = dyn_cast<ConstantInt>(InElt)) {
      unsigned Elt = CI->getZExtValue();
      if (Elt >= SrcNumElts*2)
        InElt = UndefValue::get(EltTy);
      else if (Elt >= SrcNumElts)
        InElt = GetVectorElement(V2, Elt - SrcNumElts);
      else
        InElt = GetVectorElement(V1, Elt);
      if (InElt == 0) return 0;
    } else {
      // Unknown value.
      return 0;
    }
    Result.push_back(InElt);
  }

  return ConstantVector::get(&Result[0], Result.size());
}

Constant *llvm::ConstantFoldExtractValueInstruction(Constant *Agg,
                                                    const unsigned *Idxs,
                                                    unsigned NumIdx) {
  // Base case: no indices, so return the entire value.
  if (NumIdx == 0)
    return Agg;

  if (isa<UndefValue>(Agg))  // ev(undef, x) -> undef
    return UndefValue::get(ExtractValueInst::getIndexedType(Agg->getType(),
                                                            Idxs,
                                                            Idxs + NumIdx));

  if (isa<ConstantAggregateZero>(Agg))  // ev(0, x) -> 0
    return
      Constant::getNullValue(ExtractValueInst::getIndexedType(Agg->getType(),
                                                              Idxs,
                                                              Idxs + NumIdx));

  // Otherwise recurse.
  if (ConstantStruct *CS = dyn_cast<ConstantStruct>(Agg))
    return ConstantFoldExtractValueInstruction(CS->getOperand(*Idxs),
                                               Idxs+1, NumIdx-1);

  if (ConstantArray *CA = dyn_cast<ConstantArray>(Agg))
    return ConstantFoldExtractValueInstruction(CA->getOperand(*Idxs),
                                               Idxs+1, NumIdx-1);
  ConstantVector *CV = cast<ConstantVector>(Agg);
  return ConstantFoldExtractValueInstruction(CV->getOperand(*Idxs),
                                             Idxs+1, NumIdx-1);
}

Constant *llvm::ConstantFoldInsertValueInstruction(Constant *Agg,
                                                   Constant *Val,
                                                   const unsigned *Idxs,
                                                   unsigned NumIdx) {
  // Base case: no indices, so replace the entire value.
  if (NumIdx == 0)
    return Val;

  if (isa<UndefValue>(Agg)) {
    // Insertion of constant into aggregate undef
    // Optimize away insertion of undef.
    if (isa<UndefValue>(Val))
      return Agg;
    
    // Otherwise break the aggregate undef into multiple undefs and do
    // the insertion.
    const CompositeType *AggTy = cast<CompositeType>(Agg->getType());
    unsigned numOps;
    if (const ArrayType *AR = dyn_cast<ArrayType>(AggTy))
      numOps = AR->getNumElements();
    else
      numOps = cast<StructType>(AggTy)->getNumElements();
    
    std::vector<Constant*> Ops(numOps); 
    for (unsigned i = 0; i < numOps; ++i) {
      const Type *MemberTy = AggTy->getTypeAtIndex(i);
      Constant *Op =
        (*Idxs == i) ?
        ConstantFoldInsertValueInstruction(UndefValue::get(MemberTy),
                                           Val, Idxs+1, NumIdx-1) :
        UndefValue::get(MemberTy);
      Ops[i] = Op;
    }
    
    if (const StructType* ST = dyn_cast<StructType>(AggTy))
      return ConstantStruct::get(ST->getContext(), Ops, ST->isPacked());
    return ConstantArray::get(cast<ArrayType>(AggTy), Ops);
  }
  
  if (isa<ConstantAggregateZero>(Agg)) {
    // Insertion of constant into aggregate zero
    // Optimize away insertion of zero.
    if (Val->isNullValue())
      return Agg;
    
    // Otherwise break the aggregate zero into multiple zeros and do
    // the insertion.
    const CompositeType *AggTy = cast<CompositeType>(Agg->getType());
    unsigned numOps;
    if (const ArrayType *AR = dyn_cast<ArrayType>(AggTy))
      numOps = AR->getNumElements();
    else
      numOps = cast<StructType>(AggTy)->getNumElements();
    
    std::vector<Constant*> Ops(numOps);
    for (unsigned i = 0; i < numOps; ++i) {
      const Type *MemberTy = AggTy->getTypeAtIndex(i);
      Constant *Op =
        (*Idxs == i) ?
        ConstantFoldInsertValueInstruction(Constant::getNullValue(MemberTy),
                                           Val, Idxs+1, NumIdx-1) :
        Constant::getNullValue(MemberTy);
      Ops[i] = Op;
    }
    
    if (const StructType *ST = dyn_cast<StructType>(AggTy))
      return ConstantStruct::get(ST->getContext(), Ops, ST->isPacked());
    return ConstantArray::get(cast<ArrayType>(AggTy), Ops);
  }
  
  if (isa<ConstantStruct>(Agg) || isa<ConstantArray>(Agg)) {
    // Insertion of constant into aggregate constant.
    std::vector<Constant*> Ops(Agg->getNumOperands());
    for (unsigned i = 0; i < Agg->getNumOperands(); ++i) {
      Constant *Op = cast<Constant>(Agg->getOperand(i));
      if (*Idxs == i)
        Op = ConstantFoldInsertValueInstruction(Op, Val, Idxs+1, NumIdx-1);
      Ops[i] = Op;
    }
    
    if (const StructType* ST = dyn_cast<StructType>(Agg->getType()))
      return ConstantStruct::get(ST->getContext(), Ops, ST->isPacked());
    return ConstantArray::get(cast<ArrayType>(Agg->getType()), Ops);
  }

  return 0;
}


Constant *llvm::ConstantFoldBinaryInstruction(unsigned Opcode,
                                              Constant *C1, Constant *C2) {
  // No compile-time operations on this type yet.
  if (C1->getType()->isPPC_FP128Ty())
    return 0;

  // Handle UndefValue up front.
  if (isa<UndefValue>(C1) || isa<UndefValue>(C2)) {
    switch (Opcode) {
    case Instruction::Xor:
      if (isa<UndefValue>(C1) && isa<UndefValue>(C2))
        // Handle undef ^ undef -> 0 special case. This is a common
        // idiom (misuse).
        return Constant::getNullValue(C1->getType());
      // Fallthrough
    case Instruction::Add:
    case Instruction::Sub:
      return UndefValue::get(C1->getType());
    case Instruction::Mul:
    case Instruction::And:
      return Constant::getNullValue(C1->getType());
    case Instruction::UDiv:
    case Instruction::SDiv:
    case Instruction::URem:
    case Instruction::SRem:
      if (!isa<UndefValue>(C2))                    // undef / X -> 0
        return Constant::getNullValue(C1->getType());
      return C2;                                   // X / undef -> undef
    case Instruction::Or:                          // X | undef -> -1
      if (const VectorType *PTy = dyn_cast<VectorType>(C1->getType()))
        return Constant::getAllOnesValue(PTy);
      return Constant::getAllOnesValue(C1->getType());
    case Instruction::LShr:
      if (isa<UndefValue>(C2) && isa<UndefValue>(C1))
        return C1;                                  // undef lshr undef -> undef
      return Constant::getNullValue(C1->getType()); // X lshr undef -> 0
                                                    // undef lshr X -> 0
    case Instruction::AShr:
      if (!isa<UndefValue>(C2))                     // undef ashr X --> all ones
        return Constant::getAllOnesValue(C1->getType());
      else if (isa<UndefValue>(C1)) 
        return C1;                                  // undef ashr undef -> undef
      else
        return C1;                                  // X ashr undef --> X
    case Instruction::Shl:
      // undef << X -> 0   or   X << undef -> 0
      return Constant::getNullValue(C1->getType());
    }
  }

  // Handle simplifications when the RHS is a constant int.
  if (ConstantInt *CI2 = dyn_cast<ConstantInt>(C2)) {
    switch (Opcode) {
    case Instruction::Add:
      if (CI2->equalsInt(0)) return C1;                         // X + 0 == X
      break;
    case Instruction::Sub:
      if (CI2->equalsInt(0)) return C1;                         // X - 0 == X
      break;
    case Instruction::Mul:
      if (CI2->equalsInt(0)) return C2;                         // X * 0 == 0
      if (CI2->equalsInt(1))
        return C1;                                              // X * 1 == X
      break;
    case Instruction::UDiv:
    case Instruction::SDiv:
      if (CI2->equalsInt(1))
        return C1;                                            // X / 1 == X
      if (CI2->equalsInt(0))
        return UndefValue::get(CI2->getType());               // X / 0 == undef
      break;
    case Instruction::URem:
    case Instruction::SRem:
      if (CI2->equalsInt(1))
        return Constant::getNullValue(CI2->getType());        // X % 1 == 0
      if (CI2->equalsInt(0))
        return UndefValue::get(CI2->getType());               // X % 0 == undef
      break;
    case Instruction::And:
      if (CI2->isZero()) return C2;                           // X & 0 == 0
      if (CI2->isAllOnesValue())
        return C1;                                            // X & -1 == X

      if (ConstantExpr *CE1 = dyn_cast<ConstantExpr>(C1)) {
        // (zext i32 to i64) & 4294967295 -> (zext i32 to i64)
        if (CE1->getOpcode() == Instruction::ZExt) {
          unsigned DstWidth = CI2->getType()->getBitWidth();
          unsigned SrcWidth =
            CE1->getOperand(0)->getType()->getPrimitiveSizeInBits();
          APInt PossiblySetBits(APInt::getLowBitsSet(DstWidth, SrcWidth));
          if ((PossiblySetBits & CI2->getValue()) == PossiblySetBits)
            return C1;
        }

        // If and'ing the address of a global with a constant, fold it.
        if (CE1->getOpcode() == Instruction::PtrToInt && 
            isa<GlobalValue>(CE1->getOperand(0))) {
          GlobalValue *GV = cast<GlobalValue>(CE1->getOperand(0));

          // Functions are at least 4-byte aligned.
          unsigned GVAlign = GV->getAlignment();
          if (isa<Function>(GV))
            GVAlign = std::max(GVAlign, 4U);

          if (GVAlign > 1) {
            unsigned DstWidth = CI2->getType()->getBitWidth();
            unsigned SrcWidth = std::min(DstWidth, Log2_32(GVAlign));
            APInt BitsNotSet(APInt::getLowBitsSet(DstWidth, SrcWidth));

            // If checking bits we know are clear, return zero.
            if ((CI2->getValue() & BitsNotSet) == CI2->getValue())
              return Constant::getNullValue(CI2->getType());
          }
        }
      }
      break;
    case Instruction::Or:
      if (CI2->equalsInt(0)) return C1;    // X | 0 == X
      if (CI2->isAllOnesValue())
        return C2;                         // X | -1 == -1
      break;
    case Instruction::Xor:
      if (CI2->equalsInt(0)) return C1;    // X ^ 0 == X

      if (ConstantExpr *CE1 = dyn_cast<ConstantExpr>(C1)) {
        switch (CE1->getOpcode()) {
        default: break;
        case Instruction::ICmp:
        case Instruction::FCmp:
          // cmp pred ^ true -> cmp !pred
          assert(CI2->equalsInt(1));
          CmpInst::Predicate pred = (CmpInst::Predicate)CE1->getPredicate();
          pred = CmpInst::getInversePredicate(pred);
          return ConstantExpr::getCompare(pred, CE1->getOperand(0),
                                          CE1->getOperand(1));
        }
      }
      break;
    case Instruction::AShr:
      // ashr (zext C to Ty), C2 -> lshr (zext C, CSA), C2
      if (ConstantExpr *CE1 = dyn_cast<ConstantExpr>(C1))
        if (CE1->getOpcode() == Instruction::ZExt)  // Top bits known zero.
          return ConstantExpr::getLShr(C1, C2);
      break;
    }
  } else if (isa<ConstantInt>(C1)) {
    // If C1 is a ConstantInt and C2 is not, swap the operands.
    if (Instruction::isCommutative(Opcode))
      return ConstantExpr::get(Opcode, C2, C1);
  }

  // At this point we know neither constant is an UndefValue.
  if (ConstantInt *CI1 = dyn_cast<ConstantInt>(C1)) {
    if (ConstantInt *CI2 = dyn_cast<ConstantInt>(C2)) {
      using namespace APIntOps;
      const APInt &C1V = CI1->getValue();
      const APInt &C2V = CI2->getValue();
      switch (Opcode) {
      default:
        break;
      case Instruction::Add:     
        return ConstantInt::get(CI1->getContext(), C1V + C2V);
      case Instruction::Sub:     
        return ConstantInt::get(CI1->getContext(), C1V - C2V);
      case Instruction::Mul:     
        return ConstantInt::get(CI1->getContext(), C1V * C2V);
      case Instruction::UDiv:
        assert(!CI2->isNullValue() && "Div by zero handled above");
        return ConstantInt::get(CI1->getContext(), C1V.udiv(C2V));
      case Instruction::SDiv:
        assert(!CI2->isNullValue() && "Div by zero handled above");
        if (C2V.isAllOnesValue() && C1V.isMinSignedValue())
          return UndefValue::get(CI1->getType());   // MIN_INT / -1 -> undef
        return ConstantInt::get(CI1->getContext(), C1V.sdiv(C2V));
      case Instruction::URem:
        assert(!CI2->isNullValue() && "Div by zero handled above");
        return ConstantInt::get(CI1->getContext(), C1V.urem(C2V));
      case Instruction::SRem:
        assert(!CI2->isNullValue() && "Div by zero handled above");
        if (C2V.isAllOnesValue() && C1V.isMinSignedValue())
          return UndefValue::get(CI1->getType());   // MIN_INT % -1 -> undef
        return ConstantInt::get(CI1->getContext(), C1V.srem(C2V));
      case Instruction::And:
        return ConstantInt::get(CI1->getContext(), C1V & C2V);
      case Instruction::Or:
        return ConstantInt::get(CI1->getContext(), C1V | C2V);
      case Instruction::Xor:
        return ConstantInt::get(CI1->getContext(), C1V ^ C2V);
      case Instruction::Shl: {
        uint32_t shiftAmt = C2V.getZExtValue();
        if (shiftAmt < C1V.getBitWidth())
          return ConstantInt::get(CI1->getContext(), C1V.shl(shiftAmt));
        else
          return UndefValue::get(C1->getType()); // too big shift is undef
      }
      case Instruction::LShr: {
        uint32_t shiftAmt = C2V.getZExtValue();
        if (shiftAmt < C1V.getBitWidth())
          return ConstantInt::get(CI1->getContext(), C1V.lshr(shiftAmt));
        else
          return UndefValue::get(C1->getType()); // too big shift is undef
      }
      case Instruction::AShr: {
        uint32_t shiftAmt = C2V.getZExtValue();
        if (shiftAmt < C1V.getBitWidth())
          return ConstantInt::get(CI1->getContext(), C1V.ashr(shiftAmt));
        else
          return UndefValue::get(C1->getType()); // too big shift is undef
      }
      }
    }

    switch (Opcode) {
    case Instruction::SDiv:
    case Instruction::UDiv:
    case Instruction::URem:
    case Instruction::SRem:
    case Instruction::LShr:
    case Instruction::AShr:
    case Instruction::Shl:
      if (CI1->equalsInt(0)) return C1;
      break;
    default:
      break;
    }
  } else if (ConstantFP *CFP1 = dyn_cast<ConstantFP>(C1)) {
    if (ConstantFP *CFP2 = dyn_cast<ConstantFP>(C2)) {
      APFloat C1V = CFP1->getValueAPF();
      APFloat C2V = CFP2->getValueAPF();
      APFloat C3V = C1V;  // copy for modification
      switch (Opcode) {
      default:                   
        break;
      case Instruction::FAdd:
        (void)C3V.add(C2V, APFloat::rmNearestTiesToEven);
        return ConstantFP::get(C1->getContext(), C3V);
      case Instruction::FSub:
        (void)C3V.subtract(C2V, APFloat::rmNearestTiesToEven);
        return ConstantFP::get(C1->getContext(), C3V);
      case Instruction::FMul:
        (void)C3V.multiply(C2V, APFloat::rmNearestTiesToEven);
        return ConstantFP::get(C1->getContext(), C3V);
      case Instruction::FDiv:
        (void)C3V.divide(C2V, APFloat::rmNearestTiesToEven);
        return ConstantFP::get(C1->getContext(), C3V);
      case Instruction::FRem:
        (void)C3V.mod(C2V, APFloat::rmNearestTiesToEven);
        return ConstantFP::get(C1->getContext(), C3V);
      }
    }
  } else if (const VectorType *VTy = dyn_cast<VectorType>(C1->getType())) {
    ConstantVector *CP1 = dyn_cast<ConstantVector>(C1);
    ConstantVector *CP2 = dyn_cast<ConstantVector>(C2);
    if ((CP1 != NULL || isa<ConstantAggregateZero>(C1)) &&
        (CP2 != NULL || isa<ConstantAggregateZero>(C2))) {
      std::vector<Constant*> Res;
      const Type* EltTy = VTy->getElementType();  
      Constant *C1 = 0;
      Constant *C2 = 0;
      switch (Opcode) {
      default:
        break;
      case Instruction::Add:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getAdd(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::FAdd:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getFAdd(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::Sub:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getSub(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::FSub:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getFSub(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::Mul:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getMul(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::FMul:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getFMul(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::UDiv:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getUDiv(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::SDiv:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getSDiv(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::FDiv:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getFDiv(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::URem:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getURem(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::SRem:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getSRem(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::FRem:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getFRem(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::And: 
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getAnd(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::Or:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getOr(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::Xor:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getXor(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::LShr:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getLShr(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::AShr:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getAShr(C1, C2));
        }
        return ConstantVector::get(Res);
      case Instruction::Shl:
        for (unsigned i = 0, e = VTy->getNumElements(); i != e; ++i) {
          C1 = CP1 ? CP1->getOperand(i) : Constant::getNullValue(EltTy);
          C2 = CP2 ? CP2->getOperand(i) : Constant::getNullValue(EltTy);
          Res.push_back(ConstantExpr::getShl(C1, C2));
        }
        return ConstantVector::get(Res);
      }
    }
  }

  if (ConstantExpr *CE1 = dyn_cast<ConstantExpr>(C1)) {
    // There are many possible foldings we could do here.  We should probably
    // at least fold add of a pointer with an integer into the appropriate
    // getelementptr.  This will improve alias analysis a bit.

    // Given ((a + b) + c), if (b + c) folds to something interesting, return
    // (a + (b + c)).
    if (Instruction::isAssociative(Opcode) && CE1->getOpcode() == Opcode) {
      Constant *T = ConstantExpr::get(Opcode, CE1->getOperand(1), C2);
      if (!isa<ConstantExpr>(T) || cast<ConstantExpr>(T)->getOpcode() != Opcode)
        return ConstantExpr::get(Opcode, CE1->getOperand(0), T);
    }
  } else if (isa<ConstantExpr>(C2)) {
    // If C2 is a constant expr and C1 isn't, flop them around and fold the
    // other way if possible.
    if (Instruction::isCommutative(Opcode))
      return ConstantFoldBinaryInstruction(Opcode, C2, C1);
  }

  // i1 can be simplified in many cases.
  if (C1->getType()->isIntegerTy(1)) {
    switch (Opcode) {
    case Instruction::Add:
    case Instruction::Sub:
      return ConstantExpr::getXor(C1, C2);
    case Instruction::Mul:
      return ConstantExpr::getAnd(C1, C2);
    case Instruction::Shl:
    case Instruction::LShr:
    case Instruction::AShr:
      // We can assume that C2 == 0.  If it were one the result would be
      // undefined because the shift value is as large as the bitwidth.
      return C1;
    case Instruction::SDiv:
    case Instruction::UDiv:
      // We can assume that C2 == 1.  If it were zero the result would be
      // undefined through division by zero.
      return C1;
    case Instruction::URem:
    case Instruction::SRem:
      // We can assume that C2 == 1.  If it were zero the result would be
      // undefined through division by zero.
      return ConstantInt::getFalse(C1->getContext());
    default:
      break;
    }
  }

  // We don't know how to fold this.
  return 0;
}

/// isZeroSizedType - This type is zero sized if its an array or structure of
/// zero sized types.  The only leaf zero sized type is an empty structure.
static bool isMaybeZeroSizedType(const Type *Ty) {
  if (Ty->isOpaqueTy()) return true;  // Can't say.
  if (const StructType *STy = dyn_cast<StructType>(Ty)) {

    // If all of elements have zero size, this does too.
    for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i)
      if (!isMaybeZeroSizedType(STy->getElementType(i))) return false;
    return true;

  } else if (const ArrayType *ATy = dyn_cast<ArrayType>(Ty)) {
    return isMaybeZeroSizedType(ATy->getElementType());
  }
  return false;
}

/// IdxCompare - Compare the two constants as though they were getelementptr
/// indices.  This allows coersion of the types to be the same thing.
///
/// If the two constants are the "same" (after coersion), return 0.  If the
/// first is less than the second, return -1, if the second is less than the
/// first, return 1.  If the constants are not integral, return -2.
///
static int IdxCompare(Constant *C1, Constant *C2, const Type *ElTy) {
  if (C1 == C2) return 0;

  // Ok, we found a different index.  If they are not ConstantInt, we can't do
  // anything with them.
  if (!isa<ConstantInt>(C1) || !isa<ConstantInt>(C2))
    return -2; // don't know!

  // Ok, we have two differing integer indices.  Sign extend them to be the same
  // type.  Long is always big enough, so we use it.
  if (!C1->getType()->isIntegerTy(64))
    C1 = ConstantExpr::getSExt(C1, Type::getInt64Ty(C1->getContext()));

  if (!C2->getType()->isIntegerTy(64))
    C2 = ConstantExpr::getSExt(C2, Type::getInt64Ty(C1->getContext()));

  if (C1 == C2) return 0;  // They are equal

  // If the type being indexed over is really just a zero sized type, there is
  // no pointer difference being made here.
  if (isMaybeZeroSizedType(ElTy))
    return -2; // dunno.

  // If they are really different, now that they are the same type, then we
  // found a difference!
  if (cast<ConstantInt>(C1)->getSExtValue() < 
      cast<ConstantInt>(C2)->getSExtValue())
    return -1;
  else
    return 1;
}

/// evaluateFCmpRelation - This function determines if there is anything we can
/// decide about the two constants provided.  This doesn't need to handle simple
/// things like ConstantFP comparisons, but should instead handle ConstantExprs.
/// If we can determine that the two constants have a particular relation to 
/// each other, we should return the corresponding FCmpInst predicate, 
/// otherwise return FCmpInst::BAD_FCMP_PREDICATE. This is used below in
/// ConstantFoldCompareInstruction.
///
/// To simplify this code we canonicalize the relation so that the first
/// operand is always the most "complex" of the two.  We consider ConstantFP
/// to be the simplest, and ConstantExprs to be the most complex.
static FCmpInst::Predicate evaluateFCmpRelation(Constant *V1, Constant *V2) {
  assert(V1->getType() == V2->getType() &&
         "Cannot compare values of different types!");

  // No compile-time operations on this type yet.
  if (V1->getType()->isPPC_FP128Ty())
    return FCmpInst::BAD_FCMP_PREDICATE;

  // Handle degenerate case quickly
  if (V1 == V2) return FCmpInst::FCMP_OEQ;

  if (!isa<ConstantExpr>(V1)) {
    if (!isa<ConstantExpr>(V2)) {
      // We distilled thisUse the standard constant folder for a few cases
      ConstantInt *R = 0;
      R = dyn_cast<ConstantInt>(
                      ConstantExpr::getFCmp(FCmpInst::FCMP_OEQ, V1, V2));
      if (R && !R->isZero()) 
        return FCmpInst::FCMP_OEQ;
      R = dyn_cast<ConstantInt>(
                      ConstantExpr::getFCmp(FCmpInst::FCMP_OLT, V1, V2));
      if (R && !R->isZero()) 
        return FCmpInst::FCMP_OLT;
      R = dyn_cast<ConstantInt>(
                      ConstantExpr::getFCmp(FCmpInst::FCMP_OGT, V1, V2));
      if (R && !R->isZero()) 
        return FCmpInst::FCMP_OGT;

      // Nothing more we can do
      return FCmpInst::BAD_FCMP_PREDICATE;
    }

    // If the first operand is simple and second is ConstantExpr, swap operands.
    FCmpInst::Predicate SwappedRelation = evaluateFCmpRelation(V2, V1);
    if (SwappedRelation != FCmpInst::BAD_FCMP_PREDICATE)
      return FCmpInst::getSwappedPredicate(SwappedRelation);
  } else {
    // Ok, the LHS is known to be a constantexpr.  The RHS can be any of a
    // constantexpr or a simple constant.
    ConstantExpr *CE1 = cast<ConstantExpr>(V1);
    switch (CE1->getOpcode()) {
    case Instruction::FPTrunc:
    case Instruction::FPExt:
    case Instruction::UIToFP:
    case Instruction::SIToFP:
      // We might be able to do something with these but we don't right now.
      break;
    default:
      break;
    }
  }
  // There are MANY other foldings that we could perform here.  They will
  // probably be added on demand, as they seem needed.
  return FCmpInst::BAD_FCMP_PREDICATE;
}

/// evaluateICmpRelation - This function determines if there is anything we can
/// decide about the two constants provided.  This doesn't need to handle simple
/// things like integer comparisons, but should instead handle ConstantExprs
/// and GlobalValues.  If we can determine that the two constants have a
/// particular relation to each other, we should return the corresponding ICmp
/// predicate, otherwise return ICmpInst::BAD_ICMP_PREDICATE.
///
/// To simplify this code we canonicalize the relation so that the first
/// operand is always the most "complex" of the two.  We consider simple
/// constants (like ConstantInt) to be the simplest, followed by
/// GlobalValues, followed by ConstantExpr's (the most complex).
///
static ICmpInst::Predicate evaluateICmpRelation(Constant *V1, Constant *V2,
                                                bool isSigned) {
  assert(V1->getType() == V2->getType() &&
         "Cannot compare different types of values!");
  if (V1 == V2) return ICmpInst::ICMP_EQ;

  if (!isa<ConstantExpr>(V1) && !isa<GlobalValue>(V1) &&
      !isa<BlockAddress>(V1)) {
    if (!isa<GlobalValue>(V2) && !isa<ConstantExpr>(V2) &&
        !isa<BlockAddress>(V2)) {
      // We distilled this down to a simple case, use the standard constant
      // folder.
      ConstantInt *R = 0;
      ICmpInst::Predicate pred = ICmpInst::ICMP_EQ;
      R = dyn_cast<ConstantInt>(ConstantExpr::getICmp(pred, V1, V2));
      if (R && !R->isZero()) 
        return pred;
      pred = isSigned ? ICmpInst::ICMP_SLT : ICmpInst::ICMP_ULT;
      R = dyn_cast<ConstantInt>(ConstantExpr::getICmp(pred, V1, V2));
      if (R && !R->isZero())
        return pred;
      pred = isSigned ? ICmpInst::ICMP_SGT : ICmpInst::ICMP_UGT;
      R = dyn_cast<ConstantInt>(ConstantExpr::getICmp(pred, V1, V2));
      if (R && !R->isZero())
        return pred;

      // If we couldn't figure it out, bail.
      return ICmpInst::BAD_ICMP_PREDICATE;
    }

    // If the first operand is simple, swap operands.
    ICmpInst::Predicate SwappedRelation = 
      evaluateICmpRelation(V2, V1, isSigned);
    if (SwappedRelation != ICmpInst::BAD_ICMP_PREDICATE)
      return ICmpInst::getSwappedPredicate(SwappedRelation);

  } else if (const GlobalValue *GV = dyn_cast<GlobalValue>(V1)) {
    if (isa<ConstantExpr>(V2)) {  // Swap as necessary.
      ICmpInst::Predicate SwappedRelation = 
        evaluateICmpRelation(V2, V1, isSigned);
      if (SwappedRelation != ICmpInst::BAD_ICMP_PREDICATE)
        return ICmpInst::getSwappedPredicate(SwappedRelation);
      return ICmpInst::BAD_ICMP_PREDICATE;
    }

    // Now we know that the RHS is a GlobalValue, BlockAddress or simple
    // constant (which, since the types must match, means that it's a
    // ConstantPointerNull).
    if (const GlobalValue *GV2 = dyn_cast<GlobalValue>(V2)) {
      // Don't try to decide equality of aliases.
      if (!isa<GlobalAlias>(GV) && !isa<GlobalAlias>(GV2))
        if (!GV->hasExternalWeakLinkage() || !GV2->hasExternalWeakLinkage())
          return ICmpInst::ICMP_NE;
    } else if (isa<BlockAddress>(V2)) {
      return ICmpInst::ICMP_NE; // Globals never equal labels.
    } else {
      assert(isa<ConstantPointerNull>(V2) && "Canonicalization guarantee!");
      // GlobalVals can never be null unless they have external weak linkage.
      // We don't try to evaluate aliases here.
      if (!GV->hasExternalWeakLinkage() && !isa<GlobalAlias>(GV))
        return ICmpInst::ICMP_NE;
    }
  } else if (const BlockAddress *BA = dyn_cast<BlockAddress>(V1)) {
    if (isa<ConstantExpr>(V2)) {  // Swap as necessary.
      ICmpInst::Predicate SwappedRelation = 
        evaluateICmpRelation(V2, V1, isSigned);
      if (SwappedRelation != ICmpInst::BAD_ICMP_PREDICATE)
        return ICmpInst::getSwappedPredicate(SwappedRelation);
      return ICmpInst::BAD_ICMP_PREDICATE;
    }
    
    // Now we know that the RHS is a GlobalValue, BlockAddress or simple
    // constant (which, since the types must match, means that it is a
    // ConstantPointerNull).
    if (const BlockAddress *BA2 = dyn_cast<BlockAddress>(V2)) {
      // Block address in another function can't equal this one, but block
      // addresses in the current function might be the same if blocks are
      // empty.
      if (BA2->getFunction() != BA->getFunction())
        return ICmpInst::ICMP_NE;
    } else {
      // Block addresses aren't null, don't equal the address of globals.
      assert((isa<ConstantPointerNull>(V2) || isa<GlobalValue>(V2)) &&
             "Canonicalization guarantee!");
      return ICmpInst::ICMP_NE;
    }
  } else {
    // Ok, the LHS is known to be a constantexpr.  The RHS can be any of a
    // constantexpr, a global, block address, or a simple constant.
    ConstantExpr *CE1 = cast<ConstantExpr>(V1);
    Constant *CE1Op0 = CE1->getOperand(0);

    switch (CE1->getOpcode()) {
    case Instruction::Trunc:
    case Instruction::FPTrunc:
    case Instruction::FPExt:
    case Instruction::FPToUI:
    case Instruction::FPToSI:
      break; // We can't evaluate floating point casts or truncations.

    case Instruction::UIToFP:
    case Instruction::SIToFP:
    case Instruction::BitCast:
    case Instruction::ZExt:
    case Instruction::SExt:
      // If the cast is not actually changing bits, and the second operand is a
      // null pointer, do the comparison with the pre-casted value.
      if (V2->isNullValue() &&
          (CE1->getType()->isPointerTy() || CE1->getType()->isIntegerTy())) {
        if (CE1->getOpcode() == Instruction::ZExt) isSigned = false;
        if (CE1->getOpcode() == Instruction::SExt) isSigned = true;
        return evaluateICmpRelation(CE1Op0,
                                    Constant::getNullValue(CE1Op0->getType()), 
                                    isSigned);
      }
      break;

    case Instruction::GetElementPtr:
      // Ok, since this is a getelementptr, we know that the constant has a
      // pointer type.  Check the various cases.
      if (isa<ConstantPointerNull>(V2)) {
        // If we are comparing a GEP to a null pointer, check to see if the base
        // of the GEP equals the null pointer.
        if (const GlobalValue *GV = dyn_cast<GlobalValue>(CE1Op0)) {
          if (GV->hasExternalWeakLinkage())
            // Weak linkage GVals could be zero or not. We're comparing that
            // to null pointer so its greater-or-equal
            return isSigned ? ICmpInst::ICMP_SGE : ICmpInst::ICMP_UGE;
          else 
            // If its not weak linkage, the GVal must have a non-zero address
            // so the result is greater-than
            return isSigned ? ICmpInst::ICMP_SGT : ICmpInst::ICMP_UGT;
        } else if (isa<ConstantPointerNull>(CE1Op0)) {
          // If we are indexing from a null pointer, check to see if we have any
          // non-zero indices.
          for (unsigned i = 1, e = CE1->getNumOperands(); i != e; ++i)
            if (!CE1->getOperand(i)->isNullValue())
              // Offsetting from null, must not be equal.
              return isSigned ? ICmpInst::ICMP_SGT : ICmpInst::ICMP_UGT;
          // Only zero indexes from null, must still be zero.
          return ICmpInst::ICMP_EQ;
        }
        // Otherwise, we can't really say if the first operand is null or not.
      } else if (const GlobalValue *GV2 = dyn_cast<GlobalValue>(V2)) {
        if (isa<ConstantPointerNull>(CE1Op0)) {
          if (GV2->hasExternalWeakLinkage())
            // Weak linkage GVals could be zero or not. We're comparing it to
            // a null pointer, so its less-or-equal
            return isSigned ? ICmpInst::ICMP_SLE : ICmpInst::ICMP_ULE;
          else
            // If its not weak linkage, the GVal must have a non-zero address
            // so the result is less-than
            return isSigned ? ICmpInst::ICMP_SLT : ICmpInst::ICMP_ULT;
        } else if (const GlobalValue *GV = dyn_cast<GlobalValue>(CE1Op0)) {
          if (GV == GV2) {
            // If this is a getelementptr of the same global, then it must be
            // different.  Because the types must match, the getelementptr could
            // only have at most one index, and because we fold getelementptr's
            // with a single zero index, it must be nonzero.
            assert(CE1->getNumOperands() == 2 &&
                   !CE1->getOperand(1)->isNullValue() &&
                   "Suprising getelementptr!");
            return isSigned ? ICmpInst::ICMP_SGT : ICmpInst::ICMP_UGT;
          } else {
            // If they are different globals, we don't know what the value is,
            // but they can't be equal.
            return ICmpInst::ICMP_NE;
          }
        }
      } else {
        ConstantExpr *CE2 = cast<ConstantExpr>(V2);
        Constant *CE2Op0 = CE2->getOperand(0);

        // There are MANY other foldings that we could perform here.  They will
        // probably be added on demand, as they seem needed.
        switch (CE2->getOpcode()) {
        default: break;
        case Instruction::GetElementPtr:
          // By far the most common case to handle is when the base pointers are
          // obviously to the same or different globals.
          if (isa<GlobalValue>(CE1Op0) && isa<GlobalValue>(CE2Op0)) {
            if (CE1Op0 != CE2Op0) // Don't know relative ordering, but not equal
              return ICmpInst::ICMP_NE;
            // Ok, we know that both getelementptr instructions are based on the
            // same global.  From this, we can precisely determine the relative
            // ordering of the resultant pointers.
            unsigned i = 1;

            // The logic below assumes that the result of the comparison
            // can be determined by finding the first index that differs.
            // This doesn't work if there is over-indexing in any
            // subsequent indices, so check for that case first.
            if (!CE1->isGEPWithNoNotionalOverIndexing() ||
                !CE2->isGEPWithNoNotionalOverIndexing())
               return ICmpInst::BAD_ICMP_PREDICATE; // Might be equal.

            // Compare all of the operands the GEP's have in common.
            gep_type_iterator GTI = gep_type_begin(CE1);
            for (;i != CE1->getNumOperands() && i != CE2->getNumOperands();
                 ++i, ++GTI)
              switch (IdxCompare(CE1->getOperand(i),
                                 CE2->getOperand(i), GTI.getIndexedType())) {
              case -1: return isSigned ? ICmpInst::ICMP_SLT:ICmpInst::ICMP_ULT;
              case 1:  return isSigned ? ICmpInst::ICMP_SGT:ICmpInst::ICMP_UGT;
              case -2: return ICmpInst::BAD_ICMP_PREDICATE;
              }

            // Ok, we ran out of things they have in common.  If any leftovers
            // are non-zero then we have a difference, otherwise we are equal.
            for (; i < CE1->getNumOperands(); ++i)
              if (!CE1->getOperand(i)->isNullValue()) {
                if (isa<ConstantInt>(CE1->getOperand(i)))
                  return isSigned ? ICmpInst::ICMP_SGT : ICmpInst::ICMP_UGT;
                else
                  return ICmpInst::BAD_ICMP_PREDICATE; // Might be equal.
              }

            for (; i < CE2->getNumOperands(); ++i)
              if (!CE2->getOperand(i)->isNullValue()) {
                if (isa<ConstantInt>(CE2->getOperand(i)))
                  return isSigned ? ICmpInst::ICMP_SLT : ICmpInst::ICMP_ULT;
                else
                  return ICmpInst::BAD_ICMP_PREDICATE; // Might be equal.
              }
            return ICmpInst::ICMP_EQ;
          }
        }
      }
    default:
      break;
    }
  }

  return ICmpInst::BAD_ICMP_PREDICATE;
}

Constant *llvm::ConstantFoldCompareInstruction(unsigned short pred, 
                                               Constant *C1, Constant *C2) {
  const Type *ResultTy;
  if (const VectorType *VT = dyn_cast<VectorType>(C1->getType()))
    ResultTy = VectorType::get(Type::getInt1Ty(C1->getContext()),
                               VT->getNumElements());
  else
    ResultTy = Type::getInt1Ty(C1->getContext());

  // Fold FCMP_FALSE/FCMP_TRUE unconditionally.
  if (pred == FCmpInst::FCMP_FALSE)
    return Constant::getNullValue(ResultTy);

  if (pred == FCmpInst::FCMP_TRUE)
    return Constant::getAllOnesValue(ResultTy);

  // Handle some degenerate cases first
  if (isa<UndefValue>(C1) || isa<UndefValue>(C2)) {
    // For EQ and NE, we can always pick a value for the undef to make the
    // predicate pass or fail, so we can return undef.
    if (ICmpInst::isEquality(ICmpInst::Predicate(pred)))
      return UndefValue::get(ResultTy);
    // Otherwise, pick the same value as the non-undef operand, and fold
    // it to true or false.
    return ConstantInt::get(ResultTy, CmpInst::isTrueWhenEqual(pred));
  }

  // No compile-time operations on this type yet.
  if (C1->getType()->isPPC_FP128Ty())
    return 0;

  // icmp eq/ne(null,GV) -> false/true
  if (C1->isNullValue()) {
    if (const GlobalValue *GV = dyn_cast<GlobalValue>(C2))
      // Don't try to evaluate aliases.  External weak GV can be null.
      if (!isa<GlobalAlias>(GV) && !GV->hasExternalWeakLinkage()) {
        if (pred == ICmpInst::ICMP_EQ)
          return ConstantInt::getFalse(C1->getContext());
        else if (pred == ICmpInst::ICMP_NE)
          return ConstantInt::getTrue(C1->getContext());
      }
  // icmp eq/ne(GV,null) -> false/true
  } else if (C2->isNullValue()) {
    if (const GlobalValue *GV = dyn_cast<GlobalValue>(C1))
      // Don't try to evaluate aliases.  External weak GV can be null.
      if (!isa<GlobalAlias>(GV) && !GV->hasExternalWeakLinkage()) {
        if (pred == ICmpInst::ICMP_EQ)
          return ConstantInt::getFalse(C1->getContext());
        else if (pred == ICmpInst::ICMP_NE)
          return ConstantInt::getTrue(C1->getContext());
      }
  }

  // If the comparison is a comparison between two i1's, simplify it.
  if (C1->getType()->isIntegerTy(1)) {
    switch(pred) {
    case ICmpInst::ICMP_EQ:
      if (isa<ConstantInt>(C2))
        return ConstantExpr::getXor(C1, ConstantExpr::getNot(C2));
      return ConstantExpr::getXor(ConstantExpr::getNot(C1), C2);
    case ICmpInst::ICMP_NE:
      return ConstantExpr::getXor(C1, C2);
    default:
      break;
    }
  }

  if (isa<ConstantInt>(C1) && isa<ConstantInt>(C2)) {
    APInt V1 = cast<ConstantInt>(C1)->getValue();
    APInt V2 = cast<ConstantInt>(C2)->getValue();
    switch (pred) {
    default: llvm_unreachable("Invalid ICmp Predicate"); return 0;
    case ICmpInst::ICMP_EQ:  return ConstantInt::get(ResultTy, V1 == V2);
    case ICmpInst::ICMP_NE:  return ConstantInt::get(ResultTy, V1 != V2);
    case ICmpInst::ICMP_SLT: return ConstantInt::get(ResultTy, V1.slt(V2));
    case ICmpInst::ICMP_SGT: return ConstantInt::get(ResultTy, V1.sgt(V2));
    case ICmpInst::ICMP_SLE: return ConstantInt::get(ResultTy, V1.sle(V2));
    case ICmpInst::ICMP_SGE: return ConstantInt::get(ResultTy, V1.sge(V2));
    case ICmpInst::ICMP_ULT: return ConstantInt::get(ResultTy, V1.ult(V2));
    case ICmpInst::ICMP_UGT: return ConstantInt::get(ResultTy, V1.ugt(V2));
    case ICmpInst::ICMP_ULE: return ConstantInt::get(ResultTy, V1.ule(V2));
    case ICmpInst::ICMP_UGE: return ConstantInt::get(ResultTy, V1.uge(V2));
    }
  } else if (isa<ConstantFP>(C1) && isa<ConstantFP>(C2)) {
    APFloat C1V = cast<ConstantFP>(C1)->getValueAPF();
    APFloat C2V = cast<ConstantFP>(C2)->getValueAPF();
    APFloat::cmpResult R = C1V.compare(C2V);
    switch (pred) {
    default: llvm_unreachable("Invalid FCmp Predicate"); return 0;
    case FCmpInst::FCMP_FALSE: return Constant::getNullValue(ResultTy);
    case FCmpInst::FCMP_TRUE:  return Constant::getAllOnesValue(ResultTy);
    case FCmpInst::FCMP_UNO:
      return ConstantInt::get(ResultTy, R==APFloat::cmpUnordered);
    case FCmpInst::FCMP_ORD:
      return ConstantInt::get(ResultTy, R!=APFloat::cmpUnordered);
    case FCmpInst::FCMP_UEQ:
      return ConstantInt::get(ResultTy, R==APFloat::cmpUnordered ||
                                        R==APFloat::cmpEqual);
    case FCmpInst::FCMP_OEQ:   
      return ConstantInt::get(ResultTy, R==APFloat::cmpEqual);
    case FCmpInst::FCMP_UNE:
      return ConstantInt::get(ResultTy, R!=APFloat::cmpEqual);
    case FCmpInst::FCMP_ONE:   
      return ConstantInt::get(ResultTy, R==APFloat::cmpLessThan ||
                                        R==APFloat::cmpGreaterThan);
    case FCmpInst::FCMP_ULT: 
      return ConstantInt::get(ResultTy, R==APFloat::cmpUnordered ||
                                        R==APFloat::cmpLessThan);
    case FCmpInst::FCMP_OLT:   
      return ConstantInt::get(ResultTy, R==APFloat::cmpLessThan);
    case FCmpInst::FCMP_UGT:
      return ConstantInt::get(ResultTy, R==APFloat::cmpUnordered ||
                                        R==APFloat::cmpGreaterThan);
    case FCmpInst::FCMP_OGT:
      return ConstantInt::get(ResultTy, R==APFloat::cmpGreaterThan);
    case FCmpInst::FCMP_ULE:
      return ConstantInt::get(ResultTy, R!=APFloat::cmpGreaterThan);
    case FCmpInst::FCMP_OLE: 
      return ConstantInt::get(ResultTy, R==APFloat::cmpLessThan ||
                                        R==APFloat::cmpEqual);
    case FCmpInst::FCMP_UGE:
      return ConstantInt::get(ResultTy, R!=APFloat::cmpLessThan);
    case FCmpInst::FCMP_OGE: 
      return ConstantInt::get(ResultTy, R==APFloat::cmpGreaterThan ||
                                        R==APFloat::cmpEqual);
    }
  } else if (C1->getType()->isVectorTy()) {
    SmallVector<Constant*, 16> C1Elts, C2Elts;
    C1->getVectorElements(C1Elts);
    C2->getVectorElements(C2Elts);
    if (C1Elts.empty() || C2Elts.empty())
      return 0;

    // If we can constant fold the comparison of each element, constant fold
    // the whole vector comparison.
    SmallVector<Constant*, 4> ResElts;
    for (unsigned i = 0, e = C1Elts.size(); i != e; ++i) {
      // Compare the elements, producing an i1 result or constant expr.
      ResElts.push_back(ConstantExpr::getCompare(pred, C1Elts[i], C2Elts[i]));
    }
    return ConstantVector::get(&ResElts[0], ResElts.size());
  }

  if (C1->getType()->isFloatingPointTy()) {
    int Result = -1;  // -1 = unknown, 0 = known false, 1 = known true.
    switch (evaluateFCmpRelation(C1, C2)) {
    default: llvm_unreachable("Unknown relation!");
    case FCmpInst::FCMP_UNO:
    case FCmpInst::FCMP_ORD:
    case FCmpInst::FCMP_UEQ:
    case FCmpInst::FCMP_UNE:
    case FCmpInst::FCMP_ULT:
    case FCmpInst::FCMP_UGT:
    case FCmpInst::FCMP_ULE:
    case FCmpInst::FCMP_UGE:
    case FCmpInst::FCMP_TRUE:
    case FCmpInst::FCMP_FALSE:
    case FCmpInst::BAD_FCMP_PREDICATE:
      break; // Couldn't determine anything about these constants.
    case FCmpInst::FCMP_OEQ: // We know that C1 == C2
      Result = (pred == FCmpInst::FCMP_UEQ || pred == FCmpInst::FCMP_OEQ ||
                pred == FCmpInst::FCMP_ULE || pred == FCmpInst::FCMP_OLE ||
                pred == FCmpInst::FCMP_UGE || pred == FCmpInst::FCMP_OGE);
      break;
    case FCmpInst::FCMP_OLT: // We know that C1 < C2
      Result = (pred == FCmpInst::FCMP_UNE || pred == FCmpInst::FCMP_ONE ||
                pred == FCmpInst::FCMP_ULT || pred == FCmpInst::FCMP_OLT ||
                pred == FCmpInst::FCMP_ULE || pred == FCmpInst::FCMP_OLE);
      break;
    case FCmpInst::FCMP_OGT: // We know that C1 > C2
      Result = (pred == FCmpInst::FCMP_UNE || pred == FCmpInst::FCMP_ONE ||
                pred == FCmpInst::FCMP_UGT || pred == FCmpInst::FCMP_OGT ||
                pred == FCmpInst::FCMP_UGE || pred == FCmpInst::FCMP_OGE);
      break;
    case FCmpInst::FCMP_OLE: // We know that C1 <= C2
      // We can only partially decide this relation.
      if (pred == FCmpInst::FCMP_UGT || pred == FCmpInst::FCMP_OGT) 
        Result = 0;
      else if (pred == FCmpInst::FCMP_ULT || pred == FCmpInst::FCMP_OLT) 
        Result = 1;
      break;
    case FCmpInst::FCMP_OGE: // We known that C1 >= C2
      // We can only partially decide this relation.
      if (pred == FCmpInst::FCMP_ULT || pred == FCmpInst::FCMP_OLT) 
        Result = 0;
      else if (pred == FCmpInst::FCMP_UGT || pred == FCmpInst::FCMP_OGT) 
        Result = 1;
      break;
    case FCmpInst::FCMP_ONE: // We know that C1 != C2
      // We can only partially decide this relation.
      if (pred == FCmpInst::FCMP_OEQ || pred == FCmpInst::FCMP_UEQ) 
        Result = 0;
      else if (pred == FCmpInst::FCMP_ONE || pred == FCmpInst::FCMP_UNE) 
        Result = 1;
      break;
    }

    // If we evaluated the result, return it now.
    if (Result != -1)
      return ConstantInt::get(ResultTy, Result);

  } else {
    // Evaluate the relation between the two constants, per the predicate.
    int Result = -1;  // -1 = unknown, 0 = known false, 1 = known true.
    switch (evaluateICmpRelation(C1, C2, CmpInst::isSigned(pred))) {
    default: llvm_unreachable("Unknown relational!");
    case ICmpInst::BAD_ICMP_PREDICATE:
      break;  // Couldn't determine anything about these constants.
    case ICmpInst::ICMP_EQ:   // We know the constants are equal!
      // If we know the constants are equal, we can decide the result of this
      // computation precisely.
      Result = ICmpInst::isTrueWhenEqual((ICmpInst::Predicate)pred);
      break;
    case ICmpInst::ICMP_ULT:
      switch (pred) {
      case ICmpInst::ICMP_ULT: case ICmpInst::ICMP_NE: case ICmpInst::ICMP_ULE:
        Result = 1; break;
      case ICmpInst::ICMP_UGT: case ICmpInst::ICMP_EQ: case ICmpInst::ICMP_UGE:
        Result = 0; break;
      }
      break;
    case ICmpInst::ICMP_SLT:
      switch (pred) {
      case ICmpInst::ICMP_SLT: case ICmpInst::ICMP_NE: case ICmpInst::ICMP_SLE:
        Result = 1; break;
      case ICmpInst::ICMP_SGT: case ICmpInst::ICMP_EQ: case ICmpInst::ICMP_SGE:
        Result = 0; break;
      }
      break;
    case ICmpInst::ICMP_UGT:
      switch (pred) {
      case ICmpInst::ICMP_UGT: case ICmpInst::ICMP_NE: case ICmpInst::ICMP_UGE:
        Result = 1; break;
      case ICmpInst::ICMP_ULT: case ICmpInst::ICMP_EQ: case ICmpInst::ICMP_ULE:
        Result = 0; break;
      }
      break;
    case ICmpInst::ICMP_SGT:
      switch (pred) {
      case ICmpInst::ICMP_SGT: case ICmpInst::ICMP_NE: case ICmpInst::ICMP_SGE:
        Result = 1; break;
      case ICmpInst::ICMP_SLT: case ICmpInst::ICMP_EQ: case ICmpInst::ICMP_SLE:
        Result = 0; break;
      }
      break;
    case ICmpInst::ICMP_ULE:
      if (pred == ICmpInst::ICMP_UGT) Result = 0;
      if (pred == ICmpInst::ICMP_ULT || pred == ICmpInst::ICMP_ULE) Result = 1;
      break;
    case ICmpInst::ICMP_SLE:
      if (pred == ICmpInst::ICMP_SGT) Result = 0;
      if (pred == ICmpInst::ICMP_SLT || pred == ICmpInst::ICMP_SLE) Result = 1;
      break;
    case ICmpInst::ICMP_UGE:
      if (pred == ICmpInst::ICMP_ULT) Result = 0;
      if (pred == ICmpInst::ICMP_UGT || pred == ICmpInst::ICMP_UGE) Result = 1;
      break;
    case ICmpInst::ICMP_SGE:
      if (pred == ICmpInst::ICMP_SLT) Result = 0;
      if (pred == ICmpInst::ICMP_SGT || pred == ICmpInst::ICMP_SGE) Result = 1;
      break;
    case ICmpInst::ICMP_NE:
      if (pred == ICmpInst::ICMP_EQ) Result = 0;
      if (pred == ICmpInst::ICMP_NE) Result = 1;
      break;
    }

    // If we evaluated the result, return it now.
    if (Result != -1)
      return ConstantInt::get(ResultTy, Result);

    // If the right hand side is a bitcast, try using its inverse to simplify
    // it by moving it to the left hand side.  We can't do this if it would turn
    // a vector compare into a scalar compare or visa versa.
    if (ConstantExpr *CE2 = dyn_cast<ConstantExpr>(C2)) {
      Constant *CE2Op0 = CE2->getOperand(0);
      if (CE2->getOpcode() == Instruction::BitCast &&
          CE2->getType()->isVectorTy() == CE2Op0->getType()->isVectorTy()) {
        Constant *Inverse = ConstantExpr::getBitCast(C1, CE2Op0->getType());
        return ConstantExpr::getICmp(pred, Inverse, CE2Op0);
      }
    }

    // If the left hand side is an extension, try eliminating it.
    if (ConstantExpr *CE1 = dyn_cast<ConstantExpr>(C1)) {
      if ((CE1->getOpcode() == Instruction::SExt && ICmpInst::isSigned(pred)) ||
          (CE1->getOpcode() == Instruction::ZExt && !ICmpInst::isSigned(pred))){
        Constant *CE1Op0 = CE1->getOperand(0);
        Constant *CE1Inverse = ConstantExpr::getTrunc(CE1, CE1Op0->getType());
        if (CE1Inverse == CE1Op0) {
          // Check whether we can safely truncate the right hand side.
          Constant *C2Inverse = ConstantExpr::getTrunc(C2, CE1Op0->getType());
          if (ConstantExpr::getZExt(C2Inverse, C2->getType()) == C2) {
            return ConstantExpr::getICmp(pred, CE1Inverse, C2Inverse);
          }
        }
      }
    }

    if ((!isa<ConstantExpr>(C1) && isa<ConstantExpr>(C2)) ||
        (C1->isNullValue() && !C2->isNullValue())) {
      // If C2 is a constant expr and C1 isn't, flip them around and fold the
      // other way if possible.
      // Also, if C1 is null and C2 isn't, flip them around.
      pred = ICmpInst::getSwappedPredicate((ICmpInst::Predicate)pred);
      return ConstantExpr::getICmp(pred, C2, C1);
    }
  }
  return 0;
}

/// isInBoundsIndices - Test whether the given sequence of *normalized* indices
/// is "inbounds".
template<typename IndexTy>
static bool isInBoundsIndices(IndexTy const *Idxs, size_t NumIdx) {
  // No indices means nothing that could be out of bounds.
  if (NumIdx == 0) return true;

  // If the first index is zero, it's in bounds.
  if (cast<Constant>(Idxs[0])->isNullValue()) return true;

  // If the first index is one and all the rest are zero, it's in bounds,
  // by the one-past-the-end rule.
  if (!cast<ConstantInt>(Idxs[0])->isOne())
    return false;
  for (unsigned i = 1, e = NumIdx; i != e; ++i)
    if (!cast<Constant>(Idxs[i])->isNullValue())
      return false;
  return true;
}

template<typename IndexTy>
static Constant *ConstantFoldGetElementPtrImpl(Constant *C,
                                               bool inBounds,
                                               IndexTy const *Idxs,
                                               unsigned NumIdx) {
  Constant *Idx0 = cast<Constant>(Idxs[0]);
  if (NumIdx == 0 ||
      (NumIdx == 1 && Idx0->isNullValue()))
    return C;

  if (isa<UndefValue>(C)) {
    const PointerType *Ptr = cast<PointerType>(C->getType());
    const Type *Ty = GetElementPtrInst::getIndexedType(Ptr, Idxs, Idxs+NumIdx);
    assert(Ty != 0 && "Invalid indices for GEP!");
    return UndefValue::get(PointerType::get(Ty, Ptr->getAddressSpace()));
  }

  if (C->isNullValue()) {
    bool isNull = true;
    for (unsigned i = 0, e = NumIdx; i != e; ++i)
      if (!cast<Constant>(Idxs[i])->isNullValue()) {
        isNull = false;
        break;
      }
    if (isNull) {
      const PointerType *Ptr = cast<PointerType>(C->getType());
      const Type *Ty = GetElementPtrInst::getIndexedType(Ptr, Idxs,
                                                         Idxs+NumIdx);
      assert(Ty != 0 && "Invalid indices for GEP!");
      return ConstantPointerNull::get(PointerType::get(Ty,
                                                       Ptr->getAddressSpace()));
    }
  }

  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
    // Combine Indices - If the source pointer to this getelementptr instruction
    // is a getelementptr instruction, combine the indices of the two
    // getelementptr instructions into a single instruction.
    //
    if (CE->getOpcode() == Instruction::GetElementPtr) {
      const Type *LastTy = 0;
      for (gep_type_iterator I = gep_type_begin(CE), E = gep_type_end(CE);
           I != E; ++I)
        LastTy = *I;

      if ((LastTy && LastTy->isArrayTy()) || Idx0->isNullValue()) {
        SmallVector<Value*, 16> NewIndices;
        NewIndices.reserve(NumIdx + CE->getNumOperands());
        for (unsigned i = 1, e = CE->getNumOperands()-1; i != e; ++i)
          NewIndices.push_back(CE->getOperand(i));

        // Add the last index of the source with the first index of the new GEP.
        // Make sure to handle the case when they are actually different types.
        Constant *Combined = CE->getOperand(CE->getNumOperands()-1);
        // Otherwise it must be an array.
        if (!Idx0->isNullValue()) {
          const Type *IdxTy = Combined->getType();
          if (IdxTy != Idx0->getType()) {
            const Type *Int64Ty = Type::getInt64Ty(IdxTy->getContext());
            Constant *C1 = ConstantExpr::getSExtOrBitCast(Idx0, Int64Ty);
            Constant *C2 = ConstantExpr::getSExtOrBitCast(Combined, Int64Ty);
            Combined = ConstantExpr::get(Instruction::Add, C1, C2);
          } else {
            Combined =
              ConstantExpr::get(Instruction::Add, Idx0, Combined);
          }
        }

        NewIndices.push_back(Combined);
        NewIndices.append(Idxs+1, Idxs+NumIdx);
        return (inBounds && cast<GEPOperator>(CE)->isInBounds()) ?
          ConstantExpr::getInBoundsGetElementPtr(CE->getOperand(0),
                                                 &NewIndices[0],
                                                 NewIndices.size()) :
          ConstantExpr::getGetElementPtr(CE->getOperand(0),
                                         &NewIndices[0],
                                         NewIndices.size());
      }
    }

    // Implement folding of:
    //    i32* getelementptr ([2 x i32]* bitcast ([3 x i32]* %X to [2 x i32]*),
    //                        i64 0, i64 0)
    // To: i32* getelementptr ([3 x i32]* %X, i64 0, i64 0)
    //
    if (CE->isCast() && NumIdx > 1 && Idx0->isNullValue()) {
      if (const PointerType *SPT =
          dyn_cast<PointerType>(CE->getOperand(0)->getType()))
        if (const ArrayType *SAT = dyn_cast<ArrayType>(SPT->getElementType()))
          if (const ArrayType *CAT =
        dyn_cast<ArrayType>(cast<PointerType>(C->getType())->getElementType()))
            if (CAT->getElementType() == SAT->getElementType())
              return inBounds ?
                ConstantExpr::getInBoundsGetElementPtr(
                      (Constant*)CE->getOperand(0), Idxs, NumIdx) :
                ConstantExpr::getGetElementPtr(
                      (Constant*)CE->getOperand(0), Idxs, NumIdx);
    }
  }

  // Check to see if any array indices are not within the corresponding
  // notional array bounds. If so, try to determine if they can be factored
  // out into preceding dimensions.
  bool Unknown = false;
  SmallVector<Constant *, 8> NewIdxs;
  const Type *Ty = C->getType();
  const Type *Prev = 0;
  for (unsigned i = 0; i != NumIdx;
       Prev = Ty, Ty = cast<CompositeType>(Ty)->getTypeAtIndex(Idxs[i]), ++i) {
    if (ConstantInt *CI = dyn_cast<ConstantInt>(Idxs[i])) {
      if (const ArrayType *ATy = dyn_cast<ArrayType>(Ty))
        if (ATy->getNumElements() <= INT64_MAX &&
            ATy->getNumElements() != 0 &&
            CI->getSExtValue() >= (int64_t)ATy->getNumElements()) {
          if (isa<SequentialType>(Prev)) {
            // It's out of range, but we can factor it into the prior
            // dimension.
            NewIdxs.resize(NumIdx);
            ConstantInt *Factor = ConstantInt::get(CI->getType(),
                                                   ATy->getNumElements());
            NewIdxs[i] = ConstantExpr::getSRem(CI, Factor);

            Constant *PrevIdx = cast<Constant>(Idxs[i-1]);
            Constant *Div = ConstantExpr::getSDiv(CI, Factor);

            // Before adding, extend both operands to i64 to avoid
            // overflow trouble.
            if (!PrevIdx->getType()->isIntegerTy(64))
              PrevIdx = ConstantExpr::getSExt(PrevIdx,
                                           Type::getInt64Ty(Div->getContext()));
            if (!Div->getType()->isIntegerTy(64))
              Div = ConstantExpr::getSExt(Div,
                                          Type::getInt64Ty(Div->getContext()));

            NewIdxs[i-1] = ConstantExpr::getAdd(PrevIdx, Div);
          } else {
            // It's out of range, but the prior dimension is a struct
            // so we can't do anything about it.
            Unknown = true;
          }
        }
    } else {
      // We don't know if it's in range or not.
      Unknown = true;
    }
  }

  // If we did any factoring, start over with the adjusted indices.
  if (!NewIdxs.empty()) {
    for (unsigned i = 0; i != NumIdx; ++i)
      if (!NewIdxs[i]) NewIdxs[i] = cast<Constant>(Idxs[i]);
    return inBounds ?
      ConstantExpr::getInBoundsGetElementPtr(C, NewIdxs.data(),
                                             NewIdxs.size()) :
      ConstantExpr::getGetElementPtr(C, NewIdxs.data(), NewIdxs.size());
  }

  // If all indices are known integers and normalized, we can do a simple
  // check for the "inbounds" property.
  if (!Unknown && !inBounds &&
      isa<GlobalVariable>(C) && isInBoundsIndices(Idxs, NumIdx))
    return ConstantExpr::getInBoundsGetElementPtr(C, Idxs, NumIdx);

  return 0;
}

Constant *llvm::ConstantFoldGetElementPtr(Constant *C,
                                          bool inBounds,
                                          Constant* const *Idxs,
                                          unsigned NumIdx) {
  return ConstantFoldGetElementPtrImpl(C, inBounds, Idxs, NumIdx);
}

Constant *llvm::ConstantFoldGetElementPtr(Constant *C,
                                          bool inBounds,
                                          Value* const *Idxs,
                                          unsigned NumIdx) {
  return ConstantFoldGetElementPtrImpl(C, inBounds, Idxs, NumIdx);
}
