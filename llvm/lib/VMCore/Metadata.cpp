//===-- Metadata.cpp - Implement Metadata classes -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Metadata classes.
//
//===----------------------------------------------------------------------===//

#include "LLVMContextImpl.h"
#include "llvm/Metadata.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "SymbolTableListTraitsImpl.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
//MetadataBase implementation
//

/// resizeOperands - Metadata keeps track of other metadata uses using 
/// OperandList. Resize this list to hold anticipated number of metadata
/// operands.
void MetadataBase::resizeOperands(unsigned NumOps) {
  unsigned e = getNumOperands();
  if (NumOps == 0) {
    NumOps = e*2;
    if (NumOps < 2) NumOps = 2;  
  } else if (NumOps > NumOperands) {
    // No resize needed.
    if (ReservedSpace >= NumOps) return;
  } else if (NumOps == NumOperands) {
    if (ReservedSpace == NumOps) return;
  } else {
    return;
  }

  ReservedSpace = NumOps;
  Use *OldOps = OperandList;
  Use *NewOps = allocHungoffUses(NumOps);
  std::copy(OldOps, OldOps + e, NewOps);
  OperandList = NewOps;
  if (OldOps) Use::zap(OldOps, OldOps + e, true);
}
//===----------------------------------------------------------------------===//
//MDString implementation
//
MDString *MDString::get(LLVMContext &Context, const StringRef &Str) {
  LLVMContextImpl *pImpl = Context.pImpl;
  sys::SmartScopedWriter<true> Writer(pImpl->ConstantsLock);
  StringMapEntry<MDString *> &Entry = 
    pImpl->MDStringCache.GetOrCreateValue(Str);
  MDString *&S = Entry.getValue();
  if (!S) S = new MDString(Entry.getKeyData(),
                           Entry.getKeyLength());

  return S;
}

//===----------------------------------------------------------------------===//
//MDNode implementation
//
MDNode::MDNode(Value*const* Vals, unsigned NumVals)
  : MetadataBase(Type::MetadataTy, Value::MDNodeVal) {
  NumOperands = 0;
  resizeOperands(NumVals);
  for (unsigned i = 0; i != NumVals; ++i) {
    // Only record metadata uses.
    if (MetadataBase *MB = dyn_cast_or_null<MetadataBase>(Vals[i]))
      OperandList[NumOperands++] = MB;
    Node.push_back(WeakVH(Vals[i]));
  }
}

MDNode *MDNode::get(LLVMContext &Context, Value*const* Vals, unsigned NumVals) {
  LLVMContextImpl *pImpl = Context.pImpl;
  std::vector<Value*> V;
  V.reserve(NumVals);
  for (unsigned i = 0; i < NumVals; ++i)
    V.push_back(Vals[i]);
  
  return pImpl->MDNodes.getOrCreate(Type::MetadataTy, V);
}

/// dropAllReferences - Remove all uses and clear node vector.
void MDNode::dropAllReferences() {
  User::dropAllReferences();
  Node.clear();
}

MDNode::~MDNode() {
  dropAllReferences();
}
//===----------------------------------------------------------------------===//
//NamedMDNode implementation
//
NamedMDNode::NamedMDNode(const Twine &N, MetadataBase*const* MDs, 
                         unsigned NumMDs, Module *ParentModule)
  : MetadataBase(Type::MetadataTy, Value::NamedMDNodeVal), Parent(0) {
  setName(N);
  NumOperands = 0;
  resizeOperands(NumMDs);

  for (unsigned i = 0; i != NumMDs; ++i) {
    if (MDs[i])
      OperandList[NumOperands++] = MDs[i];
    Node.push_back(WeakMetadataVH(MDs[i]));
  }
  if (ParentModule)
    ParentModule->getNamedMDList().push_back(this);
}

/// eraseFromParent - Drop all references and remove the node from parent
/// module.
void NamedMDNode::eraseFromParent() {
  getParent()->getNamedMDList().erase(this);
}

/// dropAllReferences - Remove all uses and clear node vector.
void NamedMDNode::dropAllReferences() {
  User::dropAllReferences();
  Node.clear();
}

NamedMDNode::~NamedMDNode() {
  dropAllReferences();
}
