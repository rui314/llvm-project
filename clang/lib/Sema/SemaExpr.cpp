//===--- SemaExpr.cpp - Semantic Analysis for Expressions -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for expressions.
//
//===----------------------------------------------------------------------===//

#include "Sema.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/LiteralSupport.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Parse/DeclSpec.h"
#include "clang/Parse/Designator.h"
#include "clang/Parse/Scope.h"
using namespace clang;

/// \brief Determine whether the use of this declaration is valid, and
/// emit any corresponding diagnostics.
///
/// This routine diagnoses various problems with referencing
/// declarations that can occur when using a declaration. For example,
/// it might warn if a deprecated or unavailable declaration is being
/// used, or produce an error (and return true) if a C++0x deleted
/// function is being used.
///
/// \returns true if there was an error (this declaration cannot be
/// referenced), false otherwise.
bool Sema::DiagnoseUseOfDecl(NamedDecl *D, SourceLocation Loc) {
  // See if the decl is deprecated.
  if (D->getAttr<DeprecatedAttr>()) {
    // Implementing deprecated stuff requires referencing deprecated
    // stuff. Don't warn if we are implementing a deprecated
    // construct.
    bool isSilenced = false;
    
    if (NamedDecl *ND = getCurFunctionOrMethodDecl()) {
      // If this reference happens *in* a deprecated function or method, don't
      // warn.
      isSilenced = ND->getAttr<DeprecatedAttr>();
      
      // If this is an Objective-C method implementation, check to see if the
      // method was deprecated on the declaration, not the definition.
      if (ObjCMethodDecl *MD = dyn_cast<ObjCMethodDecl>(ND)) {
        // The semantic decl context of a ObjCMethodDecl is the
        // ObjCImplementationDecl.
        if (ObjCImplementationDecl *Impl
              = dyn_cast<ObjCImplementationDecl>(MD->getParent())) {
          
          MD = Impl->getClassInterface()->getMethod(MD->getSelector(),
                                                    MD->isInstanceMethod());
          isSilenced |= MD && MD->getAttr<DeprecatedAttr>();
        }
      }
    }
    
    if (!isSilenced)
      Diag(Loc, diag::warn_deprecated) << D->getDeclName();
  }

  // See if this is a deleted function.
  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    if (FD->isDeleted()) {
      Diag(Loc, diag::err_deleted_function_use);
      Diag(D->getLocation(), diag::note_unavailable_here) << true;
      return true;
    }
  }

  // See if the decl is unavailable
  if (D->getAttr<UnavailableAttr>()) {
    Diag(Loc, diag::warn_unavailable) << D->getDeclName();
    Diag(D->getLocation(), diag::note_unavailable_here) << 0;
  }

  return false;
}

SourceRange Sema::getExprRange(ExprTy *E) const {
  Expr *Ex = (Expr *)E;
  return Ex? Ex->getSourceRange() : SourceRange();
}

//===----------------------------------------------------------------------===//
//  Standard Promotions and Conversions
//===----------------------------------------------------------------------===//

/// DefaultFunctionArrayConversion (C99 6.3.2.1p3, C99 6.3.2.1p4).
void Sema::DefaultFunctionArrayConversion(Expr *&E) {
  QualType Ty = E->getType();
  assert(!Ty.isNull() && "DefaultFunctionArrayConversion - missing type");

  if (Ty->isFunctionType())
    ImpCastExprToType(E, Context.getPointerType(Ty));
  else if (Ty->isArrayType()) {
    // In C90 mode, arrays only promote to pointers if the array expression is
    // an lvalue.  The relevant legalese is C90 6.2.2.1p3: "an lvalue that has
    // type 'array of type' is converted to an expression that has type 'pointer
    // to type'...".  In C99 this was changed to: C99 6.3.2.1p3: "an expression
    // that has type 'array of type' ...".  The relevant change is "an lvalue"
    // (C90) to "an expression" (C99).
    //
    // C++ 4.2p1:
    // An lvalue or rvalue of type "array of N T" or "array of unknown bound of
    // T" can be converted to an rvalue of type "pointer to T".
    //
    if (getLangOptions().C99 || getLangOptions().CPlusPlus ||
        E->isLvalue(Context) == Expr::LV_Valid)
      ImpCastExprToType(E, Context.getArrayDecayedType(Ty));
  }
}

/// UsualUnaryConversions - Performs various conversions that are common to most
/// operators (C99 6.3). The conversions of array and function types are 
/// sometimes surpressed. For example, the array->pointer conversion doesn't
/// apply if the array is an argument to the sizeof or address (&) operators.
/// In these instances, this routine should *not* be called.
Expr *Sema::UsualUnaryConversions(Expr *&Expr) {
  QualType Ty = Expr->getType();
  assert(!Ty.isNull() && "UsualUnaryConversions - missing type");
  
  if (Ty->isPromotableIntegerType()) // C99 6.3.1.1p2
    ImpCastExprToType(Expr, Context.IntTy);
  else
    DefaultFunctionArrayConversion(Expr);
  
  return Expr;
}

/// DefaultArgumentPromotion (C99 6.5.2.2p6). Used for function calls that
/// do not have a prototype. Arguments that have type float are promoted to 
/// double. All other argument types are converted by UsualUnaryConversions().
void Sema::DefaultArgumentPromotion(Expr *&Expr) {
  QualType Ty = Expr->getType();
  assert(!Ty.isNull() && "DefaultArgumentPromotion - missing type");
  
  // If this is a 'float' (CVR qualified or typedef) promote to double.
  if (const BuiltinType *BT = Ty->getAsBuiltinType())
    if (BT->getKind() == BuiltinType::Float)
      return ImpCastExprToType(Expr, Context.DoubleTy);
  
  UsualUnaryConversions(Expr);
}

// DefaultVariadicArgumentPromotion - Like DefaultArgumentPromotion, but
// will warn if the resulting type is not a POD type.
void Sema::DefaultVariadicArgumentPromotion(Expr *&Expr, VariadicCallType CT) {
  DefaultArgumentPromotion(Expr);
  
  if (!Expr->getType()->isPODType()) {
    Diag(Expr->getLocStart(), 
         diag::warn_cannot_pass_non_pod_arg_to_vararg) << 
    Expr->getType() << CT;
  }
}


/// UsualArithmeticConversions - Performs various conversions that are common to
/// binary operators (C99 6.3.1.8). If both operands aren't arithmetic, this
/// routine returns the first non-arithmetic type found. The client is 
/// responsible for emitting appropriate error diagnostics.
/// FIXME: verify the conversion rules for "complex int" are consistent with
/// GCC.
QualType Sema::UsualArithmeticConversions(Expr *&lhsExpr, Expr *&rhsExpr,
                                          bool isCompAssign) {
  if (!isCompAssign) {
    UsualUnaryConversions(lhsExpr);
    UsualUnaryConversions(rhsExpr);
  }

  // For conversion purposes, we ignore any qualifiers. 
  // For example, "const float" and "float" are equivalent.
  QualType lhs =
    Context.getCanonicalType(lhsExpr->getType()).getUnqualifiedType();
  QualType rhs = 
    Context.getCanonicalType(rhsExpr->getType()).getUnqualifiedType();

  // If both types are identical, no conversion is needed.
  if (lhs == rhs)
    return lhs;

  // If either side is a non-arithmetic type (e.g. a pointer), we are done.
  // The caller can deal with this (e.g. pointer + int).
  if (!lhs->isArithmeticType() || !rhs->isArithmeticType())
    return lhs;

  QualType destType = UsualArithmeticConversionsType(lhs, rhs);
  if (!isCompAssign) {
    ImpCastExprToType(lhsExpr, destType);
    ImpCastExprToType(rhsExpr, destType);
  }
  return destType;
}

QualType Sema::UsualArithmeticConversionsType(QualType lhs, QualType rhs) {
  // Perform the usual unary conversions. We do this early so that
  // integral promotions to "int" can allow us to exit early, in the
  // lhs == rhs check. Also, for conversion purposes, we ignore any
  // qualifiers.  For example, "const float" and "float" are
  // equivalent.
  if (lhs->isPromotableIntegerType())
    lhs = Context.IntTy;
  else
    lhs = lhs.getUnqualifiedType();
  if (rhs->isPromotableIntegerType())
    rhs = Context.IntTy;
  else
    rhs = rhs.getUnqualifiedType();

  // If both types are identical, no conversion is needed.
  if (lhs == rhs)
    return lhs;
  
  // If either side is a non-arithmetic type (e.g. a pointer), we are done.
  // The caller can deal with this (e.g. pointer + int).
  if (!lhs->isArithmeticType() || !rhs->isArithmeticType())
    return lhs;
    
  // At this point, we have two different arithmetic types. 
  
  // Handle complex types first (C99 6.3.1.8p1).
  if (lhs->isComplexType() || rhs->isComplexType()) {
    // if we have an integer operand, the result is the complex type.
    if (rhs->isIntegerType() || rhs->isComplexIntegerType()) { 
      // convert the rhs to the lhs complex type.
      return lhs;
    }
    if (lhs->isIntegerType() || lhs->isComplexIntegerType()) { 
      // convert the lhs to the rhs complex type.
      return rhs;
    }
    // This handles complex/complex, complex/float, or float/complex.
    // When both operands are complex, the shorter operand is converted to the 
    // type of the longer, and that is the type of the result. This corresponds 
    // to what is done when combining two real floating-point operands. 
    // The fun begins when size promotion occur across type domains. 
    // From H&S 6.3.4: When one operand is complex and the other is a real
    // floating-point type, the less precise type is converted, within it's 
    // real or complex domain, to the precision of the other type. For example,
    // when combining a "long double" with a "double _Complex", the 
    // "double _Complex" is promoted to "long double _Complex".
    int result = Context.getFloatingTypeOrder(lhs, rhs);
    
    if (result > 0) { // The left side is bigger, convert rhs. 
      rhs = Context.getFloatingTypeOfSizeWithinDomain(lhs, rhs);
    } else if (result < 0) { // The right side is bigger, convert lhs. 
      lhs = Context.getFloatingTypeOfSizeWithinDomain(rhs, lhs);
    } 
    // At this point, lhs and rhs have the same rank/size. Now, make sure the
    // domains match. This is a requirement for our implementation, C99
    // does not require this promotion.
    if (lhs != rhs) { // Domains don't match, we have complex/float mix.
      if (lhs->isRealFloatingType()) { // handle "double, _Complex double".
        return rhs;
      } else { // handle "_Complex double, double".
        return lhs;
      }
    }
    return lhs; // The domain/size match exactly.
  }
  // Now handle "real" floating types (i.e. float, double, long double).
  if (lhs->isRealFloatingType() || rhs->isRealFloatingType()) {
    // if we have an integer operand, the result is the real floating type.
    if (rhs->isIntegerType()) {
      // convert rhs to the lhs floating point type.
      return lhs;
    }
    if (rhs->isComplexIntegerType()) {
      // convert rhs to the complex floating point type.
      return Context.getComplexType(lhs);
    }
    if (lhs->isIntegerType()) {
      // convert lhs to the rhs floating point type.
      return rhs;
    }
    if (lhs->isComplexIntegerType()) { 
      // convert lhs to the complex floating point type.
      return Context.getComplexType(rhs);
    }
    // We have two real floating types, float/complex combos were handled above.
    // Convert the smaller operand to the bigger result.
    int result = Context.getFloatingTypeOrder(lhs, rhs);
    if (result > 0) // convert the rhs
      return lhs;
    assert(result < 0 && "illegal float comparison");
    return rhs;   // convert the lhs
  }
  if (lhs->isComplexIntegerType() || rhs->isComplexIntegerType()) {
    // Handle GCC complex int extension.
    const ComplexType *lhsComplexInt = lhs->getAsComplexIntegerType();
    const ComplexType *rhsComplexInt = rhs->getAsComplexIntegerType();

    if (lhsComplexInt && rhsComplexInt) {
      if (Context.getIntegerTypeOrder(lhsComplexInt->getElementType(), 
                                      rhsComplexInt->getElementType()) >= 0)
        return lhs; // convert the rhs
      return rhs;
    } else if (lhsComplexInt && rhs->isIntegerType()) {
      // convert the rhs to the lhs complex type.
      return lhs;
    } else if (rhsComplexInt && lhs->isIntegerType()) {
      // convert the lhs to the rhs complex type.
      return rhs;
    }
  }
  // Finally, we have two differing integer types.
  // The rules for this case are in C99 6.3.1.8
  int compare = Context.getIntegerTypeOrder(lhs, rhs);
  bool lhsSigned = lhs->isSignedIntegerType(),
       rhsSigned = rhs->isSignedIntegerType();
  QualType destType;
  if (lhsSigned == rhsSigned) {
    // Same signedness; use the higher-ranked type
    destType = compare >= 0 ? lhs : rhs;
  } else if (compare != (lhsSigned ? 1 : -1)) {
    // The unsigned type has greater than or equal rank to the
    // signed type, so use the unsigned type
    destType = lhsSigned ? rhs : lhs;
  } else if (Context.getIntWidth(lhs) != Context.getIntWidth(rhs)) {
    // The two types are different widths; if we are here, that
    // means the signed type is larger than the unsigned type, so
    // use the signed type.
    destType = lhsSigned ? lhs : rhs;
  } else {
    // The signed type is higher-ranked than the unsigned type,
    // but isn't actually any bigger (like unsigned int and long
    // on most 32-bit systems).  Use the unsigned type corresponding
    // to the signed type.
    destType = Context.getCorrespondingUnsignedType(lhsSigned ? lhs : rhs);
  }
  return destType;
}

//===----------------------------------------------------------------------===//
//  Semantic Analysis for various Expression Types
//===----------------------------------------------------------------------===//


/// ActOnStringLiteral - The specified tokens were lexed as pasted string
/// fragments (e.g. "foo" "bar" L"baz").  The result string has to handle string
/// concatenation ([C99 5.1.1.2, translation phase #6]), so it may come from
/// multiple tokens.  However, the common case is that StringToks points to one
/// string.
///
Action::OwningExprResult
Sema::ActOnStringLiteral(const Token *StringToks, unsigned NumStringToks) {
  assert(NumStringToks && "Must have at least one string!");

  StringLiteralParser Literal(StringToks, NumStringToks, PP);
  if (Literal.hadError)
    return ExprError();

  llvm::SmallVector<SourceLocation, 4> StringTokLocs;
  for (unsigned i = 0; i != NumStringToks; ++i)
    StringTokLocs.push_back(StringToks[i].getLocation());

  QualType StrTy = Context.CharTy;
  if (Literal.AnyWide) StrTy = Context.getWCharType();
  if (Literal.Pascal) StrTy = Context.UnsignedCharTy;

  // A C++ string literal has a const-qualified element type (C++ 2.13.4p1).
  if (getLangOptions().CPlusPlus)
    StrTy.addConst();

  // Get an array type for the string, according to C99 6.4.5.  This includes
  // the nul terminator character as well as the string length for pascal
  // strings.
  StrTy = Context.getConstantArrayType(StrTy,
                                 llvm::APInt(32, Literal.GetNumStringChars()+1),
                                       ArrayType::Normal, 0);
  
  // Pass &StringTokLocs[0], StringTokLocs.size() to factory!
  return Owned(StringLiteral::Create(Context, Literal.GetString(), 
                                     Literal.GetStringLength(),
                                     Literal.AnyWide, StrTy,
                                     &StringTokLocs[0],
                                     StringTokLocs.size()));
}

/// ShouldSnapshotBlockValueReference - Return true if a reference inside of
/// CurBlock to VD should cause it to be snapshotted (as we do for auto
/// variables defined outside the block) or false if this is not needed (e.g.
/// for values inside the block or for globals).
///
/// FIXME: This will create BlockDeclRefExprs for global variables,
/// function references, etc which is suboptimal :) and breaks
/// things like "integer constant expression" tests.
static bool ShouldSnapshotBlockValueReference(BlockSemaInfo *CurBlock,
                                              ValueDecl *VD) {
  // If the value is defined inside the block, we couldn't snapshot it even if
  // we wanted to.
  if (CurBlock->TheDecl == VD->getDeclContext())
    return false;
  
  // If this is an enum constant or function, it is constant, don't snapshot.
  if (isa<EnumConstantDecl>(VD) || isa<FunctionDecl>(VD))
    return false;

  // If this is a reference to an extern, static, or global variable, no need to
  // snapshot it.
  // FIXME: What about 'const' variables in C++?
  if (const VarDecl *Var = dyn_cast<VarDecl>(VD))
    return Var->hasLocalStorage();
  
  return true;
}  
    


/// ActOnIdentifierExpr - The parser read an identifier in expression context,
/// validate it per-C99 6.5.1.  HasTrailingLParen indicates whether this
/// identifier is used in a function call context.
/// SS is only used for a C++ qualified-id (foo::bar) to indicate the
/// class or namespace that the identifier must be a member of.
Sema::OwningExprResult Sema::ActOnIdentifierExpr(Scope *S, SourceLocation Loc,
                                                 IdentifierInfo &II,
                                                 bool HasTrailingLParen,
                                                 const CXXScopeSpec *SS,
                                                 bool isAddressOfOperand) {
  return ActOnDeclarationNameExpr(S, Loc, &II, HasTrailingLParen, SS,
                                  isAddressOfOperand);
}

/// BuildDeclRefExpr - Build either a DeclRefExpr or a
/// QualifiedDeclRefExpr based on whether or not SS is a
/// nested-name-specifier.
DeclRefExpr *
Sema::BuildDeclRefExpr(NamedDecl *D, QualType Ty, SourceLocation Loc,
                       bool TypeDependent, bool ValueDependent,
                       const CXXScopeSpec *SS) {
  if (SS && !SS->isEmpty())
    return new (Context) QualifiedDeclRefExpr(D, Ty, Loc, TypeDependent, 
                                              ValueDependent,
                                              SS->getRange().getBegin());
  else
    return new (Context) DeclRefExpr(D, Ty, Loc, TypeDependent, ValueDependent);
}

/// getObjectForAnonymousRecordDecl - Retrieve the (unnamed) field or
/// variable corresponding to the anonymous union or struct whose type
/// is Record.
static Decl *getObjectForAnonymousRecordDecl(RecordDecl *Record) {
  assert(Record->isAnonymousStructOrUnion() && 
         "Record must be an anonymous struct or union!");
  
  // FIXME: Once Decls are directly linked together, this will
  // be an O(1) operation rather than a slow walk through DeclContext's
  // vector (which itself will be eliminated). DeclGroups might make
  // this even better.
  DeclContext *Ctx = Record->getDeclContext();
  for (DeclContext::decl_iterator D = Ctx->decls_begin(), 
                               DEnd = Ctx->decls_end();
       D != DEnd; ++D) {
    if (*D == Record) {
      // The object for the anonymous struct/union directly
      // follows its type in the list of declarations.
      ++D;
      assert(D != DEnd && "Missing object for anonymous record");
      assert(!cast<NamedDecl>(*D)->getDeclName() && "Decl should be unnamed");
      return *D;
    }
  }

  assert(false && "Missing object for anonymous record");
  return 0;
}

Sema::OwningExprResult
Sema::BuildAnonymousStructUnionMemberReference(SourceLocation Loc,
                                               FieldDecl *Field,
                                               Expr *BaseObjectExpr,
                                               SourceLocation OpLoc) {
  assert(Field->getDeclContext()->isRecord() &&
         cast<RecordDecl>(Field->getDeclContext())->isAnonymousStructOrUnion()
         && "Field must be stored inside an anonymous struct or union");

  // Construct the sequence of field member references
  // we'll have to perform to get to the field in the anonymous
  // union/struct. The list of members is built from the field
  // outward, so traverse it backwards to go from an object in
  // the current context to the field we found.
  llvm::SmallVector<FieldDecl *, 4> AnonFields;
  AnonFields.push_back(Field);
  VarDecl *BaseObject = 0;
  DeclContext *Ctx = Field->getDeclContext();
  do {
    RecordDecl *Record = cast<RecordDecl>(Ctx);
    Decl *AnonObject = getObjectForAnonymousRecordDecl(Record);
    if (FieldDecl *AnonField = dyn_cast<FieldDecl>(AnonObject))
      AnonFields.push_back(AnonField);
    else {
      BaseObject = cast<VarDecl>(AnonObject);
      break;
    }
    Ctx = Ctx->getParent();
  } while (Ctx->isRecord() && 
           cast<RecordDecl>(Ctx)->isAnonymousStructOrUnion());
  
  // Build the expression that refers to the base object, from
  // which we will build a sequence of member references to each
  // of the anonymous union objects and, eventually, the field we
  // found via name lookup.
  bool BaseObjectIsPointer = false;
  unsigned ExtraQuals = 0;
  if (BaseObject) {
    // BaseObject is an anonymous struct/union variable (and is,
    // therefore, not part of another non-anonymous record).
    if (BaseObjectExpr) BaseObjectExpr->Destroy(Context);
    BaseObjectExpr = new (Context) DeclRefExpr(BaseObject,BaseObject->getType(),
                                               SourceLocation());
    ExtraQuals 
      = Context.getCanonicalType(BaseObject->getType()).getCVRQualifiers();
  } else if (BaseObjectExpr) {
    // The caller provided the base object expression. Determine
    // whether its a pointer and whether it adds any qualifiers to the
    // anonymous struct/union fields we're looking into.
    QualType ObjectType = BaseObjectExpr->getType();
    if (const PointerType *ObjectPtr = ObjectType->getAsPointerType()) {
      BaseObjectIsPointer = true;
      ObjectType = ObjectPtr->getPointeeType();
    }
    ExtraQuals = Context.getCanonicalType(ObjectType).getCVRQualifiers();
  } else {
    // We've found a member of an anonymous struct/union that is
    // inside a non-anonymous struct/union, so in a well-formed
    // program our base object expression is "this".
    if (CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(CurContext)) {
      if (!MD->isStatic()) {
        QualType AnonFieldType 
          = Context.getTagDeclType(
                     cast<RecordDecl>(AnonFields.back()->getDeclContext()));
        QualType ThisType = Context.getTagDeclType(MD->getParent());
        if ((Context.getCanonicalType(AnonFieldType) 
               == Context.getCanonicalType(ThisType)) ||
            IsDerivedFrom(ThisType, AnonFieldType)) {
          // Our base object expression is "this".
          BaseObjectExpr = new (Context) CXXThisExpr(SourceLocation(),
                                                     MD->getThisType(Context));
          BaseObjectIsPointer = true;
        }
      } else {
        return ExprError(Diag(Loc,diag::err_invalid_member_use_in_static_method)
          << Field->getDeclName());
      }
      ExtraQuals = MD->getTypeQualifiers();
    }

    if (!BaseObjectExpr) 
      return ExprError(Diag(Loc, diag::err_invalid_non_static_member_use)
        << Field->getDeclName());
  }

  // Build the implicit member references to the field of the
  // anonymous struct/union.
  Expr *Result = BaseObjectExpr;
  for (llvm::SmallVector<FieldDecl *, 4>::reverse_iterator
         FI = AnonFields.rbegin(), FIEnd = AnonFields.rend();
       FI != FIEnd; ++FI) {
    QualType MemberType = (*FI)->getType();
    if (!(*FI)->isMutable()) {
      unsigned combinedQualifiers 
        = MemberType.getCVRQualifiers() | ExtraQuals;
      MemberType = MemberType.getQualifiedType(combinedQualifiers);
    }
    Result = new (Context) MemberExpr(Result, BaseObjectIsPointer, *FI,
                                      OpLoc, MemberType);
    BaseObjectIsPointer = false;
    ExtraQuals = Context.getCanonicalType(MemberType).getCVRQualifiers();
  }

  return Owned(Result);
}

/// ActOnDeclarationNameExpr - The parser has read some kind of name
/// (e.g., a C++ id-expression (C++ [expr.prim]p1)). This routine
/// performs lookup on that name and returns an expression that refers
/// to that name. This routine isn't directly called from the parser,
/// because the parser doesn't know about DeclarationName. Rather,
/// this routine is called by ActOnIdentifierExpr,
/// ActOnOperatorFunctionIdExpr, and ActOnConversionFunctionExpr,
/// which form the DeclarationName from the corresponding syntactic
/// forms.
///
/// HasTrailingLParen indicates whether this identifier is used in a
/// function call context.  LookupCtx is only used for a C++
/// qualified-id (foo::bar) to indicate the class or namespace that
/// the identifier must be a member of.
///
/// isAddressOfOperand means that this expression is the direct operand
/// of an address-of operator. This matters because this is the only
/// situation where a qualified name referencing a non-static member may
/// appear outside a member function of this class.
Sema::OwningExprResult
Sema::ActOnDeclarationNameExpr(Scope *S, SourceLocation Loc,
                               DeclarationName Name, bool HasTrailingLParen,
                               const CXXScopeSpec *SS, 
                               bool isAddressOfOperand) {
  // Could be enum-constant, value decl, instance variable, etc.
  if (SS && SS->isInvalid())
    return ExprError();
  LookupResult Lookup = LookupParsedName(S, SS, Name, LookupOrdinaryName,
                                         false, true, Loc);

  NamedDecl *D = 0;
  if (Lookup.isAmbiguous()) {
    DiagnoseAmbiguousLookup(Lookup, Name, Loc,
                            SS && SS->isSet() ? SS->getRange()
                                              : SourceRange());
    return ExprError();
  } else
    D = Lookup.getAsDecl();

  // If this reference is in an Objective-C method, then ivar lookup happens as
  // well.
  IdentifierInfo *II = Name.getAsIdentifierInfo();
  if (II && getCurMethodDecl()) {
    // There are two cases to handle here.  1) scoped lookup could have failed,
    // in which case we should look for an ivar.  2) scoped lookup could have
    // found a decl, but that decl is outside the current instance method (i.e. 
    // a global variable).  In these two cases, we do a lookup for an ivar with 
    // this name, if the lookup sucedes, we replace it our current decl.
    if (D == 0 || D->isDefinedOutsideFunctionOrMethod()) {
      ObjCInterfaceDecl *IFace = getCurMethodDecl()->getClassInterface();
      ObjCInterfaceDecl *ClassDeclared;
      if (ObjCIvarDecl *IV = IFace->lookupInstanceVariable(II, ClassDeclared)) {
        // Check if referencing a field with __attribute__((deprecated)).
        if (DiagnoseUseOfDecl(IV, Loc))
          return ExprError();
        bool IsClsMethod = getCurMethodDecl()->isClassMethod();
        // If a class method attemps to use a free standing ivar, this is
        // an error.
        if (IsClsMethod && D && !D->isDefinedOutsideFunctionOrMethod())
           return ExprError(Diag(Loc, diag::error_ivar_use_in_class_method)
                           << IV->getDeclName());
        // If a class method uses a global variable, even if an ivar with
        // same name exists, use the global.
        if (!IsClsMethod) {
          if (IV->getAccessControl() == ObjCIvarDecl::Private &&
              ClassDeclared != IFace)
           Diag(Loc, diag::error_private_ivar_access) << IV->getDeclName();
          // FIXME: This should use a new expr for a direct reference, don't turn
          // this into Self->ivar, just return a BareIVarExpr or something.
          IdentifierInfo &II = Context.Idents.get("self");
          OwningExprResult SelfExpr = ActOnIdentifierExpr(S, Loc, II, false);
          ObjCIvarRefExpr *MRef = new (Context) ObjCIvarRefExpr(IV, IV->getType(),
                                    Loc, static_cast<Expr*>(SelfExpr.release()),
                                    true, true);
          Context.setFieldDecl(IFace, IV, MRef);
          return Owned(MRef);
        }
      }
    }
    else if (getCurMethodDecl()->isInstanceMethod()) {
      // We should warn if a local variable hides an ivar.
      ObjCInterfaceDecl *IFace = getCurMethodDecl()->getClassInterface();
      ObjCInterfaceDecl *ClassDeclared;
      if (ObjCIvarDecl *IV = IFace->lookupInstanceVariable(II, ClassDeclared)) {
        if (IV->getAccessControl() != ObjCIvarDecl::Private ||
            IFace == ClassDeclared)
          Diag(Loc, diag::warn_ivar_use_hidden)<<IV->getDeclName();
      }
    }
    // Needed to implement property "super.method" notation.
    if (D == 0 && II->isStr("super")) {
      QualType T;
      
      if (getCurMethodDecl()->isInstanceMethod())
        T = Context.getPointerType(Context.getObjCInterfaceType(
                                   getCurMethodDecl()->getClassInterface()));
      else
        T = Context.getObjCClassType();
      return Owned(new (Context) ObjCSuperExpr(Loc, T));
    }
  }

  // Determine whether this name might be a candidate for
  // argument-dependent lookup.
  bool ADL = getLangOptions().CPlusPlus && (!SS || !SS->isSet()) && 
             HasTrailingLParen;

  if (ADL && D == 0) {
    // We've seen something of the form
    //
    //   identifier(
    //
    // and we did not find any entity by the name
    // "identifier". However, this identifier is still subject to
    // argument-dependent lookup, so keep track of the name.
    return Owned(new (Context) UnresolvedFunctionNameExpr(Name,
                                                          Context.OverloadTy,
                                                          Loc));
  }

  if (D == 0) {
    // Otherwise, this could be an implicitly declared function reference (legal
    // in C90, extension in C99).
    if (HasTrailingLParen && II &&
        !getLangOptions().CPlusPlus) // Not in C++.
      D = ImplicitlyDefineFunction(Loc, *II, S);
    else {
      // If this name wasn't predeclared and if this is not a function call,
      // diagnose the problem.
      if (SS && !SS->isEmpty())
        return ExprError(Diag(Loc, diag::err_typecheck_no_member)
          << Name << SS->getRange());
      else if (Name.getNameKind() == DeclarationName::CXXOperatorName ||
               Name.getNameKind() == DeclarationName::CXXConversionFunctionName)
        return ExprError(Diag(Loc, diag::err_undeclared_use)
          << Name.getAsString());
      else
        return ExprError(Diag(Loc, diag::err_undeclared_var_use) << Name);
    }
  }

  // If this is an expression of the form &Class::member, don't build an
  // implicit member ref, because we want a pointer to the member in general,
  // not any specific instance's member.
  if (isAddressOfOperand && SS && !SS->isEmpty() && !HasTrailingLParen) {
    DeclContext *DC = static_cast<DeclContext*>(SS->getScopeRep());
    if (D && isa<CXXRecordDecl>(DC)) {
      QualType DType;
      if (FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
        DType = FD->getType().getNonReferenceType();
      } else if (CXXMethodDecl *Method = dyn_cast<CXXMethodDecl>(D)) {
        DType = Method->getType();
      } else if (isa<OverloadedFunctionDecl>(D)) {
        DType = Context.OverloadTy;
      }
      // Could be an inner type. That's diagnosed below, so ignore it here.
      if (!DType.isNull()) {
        // The pointer is type- and value-dependent if it points into something
        // dependent.
        bool Dependent = false;
        for (; DC; DC = DC->getParent()) {
          // FIXME: could stop early at namespace scope.
          if (DC->isRecord()) {
            CXXRecordDecl *Record = cast<CXXRecordDecl>(DC);
            if (Context.getTypeDeclType(Record)->isDependentType()) {
              Dependent = true;
              break;
            }
          }
        }
        return Owned(BuildDeclRefExpr(D, DType, Loc, Dependent, Dependent, SS));
      }
    }
  }

  // We may have found a field within an anonymous union or struct
  // (C++ [class.union]).
  if (FieldDecl *FD = dyn_cast<FieldDecl>(D))
    if (cast<RecordDecl>(FD->getDeclContext())->isAnonymousStructOrUnion())
      return BuildAnonymousStructUnionMemberReference(Loc, FD);

  if (CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(CurContext)) {
    if (!MD->isStatic()) {
      // C++ [class.mfct.nonstatic]p2: 
      //   [...] if name lookup (3.4.1) resolves the name in the
      //   id-expression to a nonstatic nontype member of class X or of
      //   a base class of X, the id-expression is transformed into a
      //   class member access expression (5.2.5) using (*this) (9.3.2)
      //   as the postfix-expression to the left of the '.' operator.
      DeclContext *Ctx = 0;
      QualType MemberType;
      if (FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
        Ctx = FD->getDeclContext();
        MemberType = FD->getType();

        if (const ReferenceType *RefType = MemberType->getAsReferenceType())
          MemberType = RefType->getPointeeType();
        else if (!FD->isMutable()) {
          unsigned combinedQualifiers 
            = MemberType.getCVRQualifiers() | MD->getTypeQualifiers();
          MemberType = MemberType.getQualifiedType(combinedQualifiers);
        }
      } else if (CXXMethodDecl *Method = dyn_cast<CXXMethodDecl>(D)) {
        if (!Method->isStatic()) {
          Ctx = Method->getParent();
          MemberType = Method->getType();
        }
      } else if (OverloadedFunctionDecl *Ovl 
                   = dyn_cast<OverloadedFunctionDecl>(D)) {
        for (OverloadedFunctionDecl::function_iterator 
               Func = Ovl->function_begin(),
               FuncEnd = Ovl->function_end();
             Func != FuncEnd; ++Func) {
          if (CXXMethodDecl *DMethod = dyn_cast<CXXMethodDecl>(*Func))
            if (!DMethod->isStatic()) {
              Ctx = Ovl->getDeclContext();
              MemberType = Context.OverloadTy;
              break;
            }
        }
      }

      if (Ctx && Ctx->isRecord()) {
        QualType CtxType = Context.getTagDeclType(cast<CXXRecordDecl>(Ctx));
        QualType ThisType = Context.getTagDeclType(MD->getParent());
        if ((Context.getCanonicalType(CtxType) 
               == Context.getCanonicalType(ThisType)) ||
            IsDerivedFrom(ThisType, CtxType)) {
          // Build the implicit member access expression.
          Expr *This = new (Context) CXXThisExpr(SourceLocation(),
                                                 MD->getThisType(Context));
          return Owned(new (Context) MemberExpr(This, true, D,
                                                SourceLocation(), MemberType));
        }
      }
    }
  }

  if (FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
    if (CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(CurContext)) {
      if (MD->isStatic())
        // "invalid use of member 'x' in static member function"
        return ExprError(Diag(Loc,diag::err_invalid_member_use_in_static_method)
          << FD->getDeclName());
    }

    // Any other ways we could have found the field in a well-formed
    // program would have been turned into implicit member expressions
    // above.
    return ExprError(Diag(Loc, diag::err_invalid_non_static_member_use)
      << FD->getDeclName());
  }

  if (isa<TypedefDecl>(D))
    return ExprError(Diag(Loc, diag::err_unexpected_typedef) << Name);
  if (isa<ObjCInterfaceDecl>(D))
    return ExprError(Diag(Loc, diag::err_unexpected_interface) << Name);
  if (isa<NamespaceDecl>(D))
    return ExprError(Diag(Loc, diag::err_unexpected_namespace) << Name);

  // Make the DeclRefExpr or BlockDeclRefExpr for the decl.
  if (OverloadedFunctionDecl *Ovl = dyn_cast<OverloadedFunctionDecl>(D))
    return Owned(BuildDeclRefExpr(Ovl, Context.OverloadTy, Loc,
                                  false, false, SS));
  else if (TemplateDecl *Template = dyn_cast<TemplateDecl>(D))
    return Owned(BuildDeclRefExpr(Template, Context.OverloadTy, Loc,
                                  false, false, SS));
  ValueDecl *VD = cast<ValueDecl>(D);

  // Check whether this declaration can be used. Note that we suppress
  // this check when we're going to perform argument-dependent lookup
  // on this function name, because this might not be the function
  // that overload resolution actually selects.
  if (!(ADL && isa<FunctionDecl>(VD)) && DiagnoseUseOfDecl(VD, Loc))
    return ExprError();

  if (VarDecl *Var = dyn_cast<VarDecl>(VD)) {
    // Warn about constructs like:
    //   if (void *X = foo()) { ... } else { X }.
    // In the else block, the pointer is always false.
    if (Var->isDeclaredInCondition() && Var->getType()->isScalarType()) {
      Scope *CheckS = S;
      while (CheckS) {
        if (CheckS->isWithinElse() && 
            CheckS->getControlParent()->isDeclScope(Var)) {
          if (Var->getType()->isBooleanType())
            ExprError(Diag(Loc, diag::warn_value_always_false)
              << Var->getDeclName());
          else
            ExprError(Diag(Loc, diag::warn_value_always_zero)
              << Var->getDeclName());
          break;
        }

        // Move up one more control parent to check again.
        CheckS = CheckS->getControlParent();
        if (CheckS)
          CheckS = CheckS->getParent();
      }
    }
  } else if (FunctionDecl *Func = dyn_cast<FunctionDecl>(VD)) {
    if (!getLangOptions().CPlusPlus && !Func->hasPrototype()) {
      // C99 DR 316 says that, if a function type comes from a
      // function definition (without a prototype), that type is only
      // used for checking compatibility. Therefore, when referencing
      // the function, we pretend that we don't have the full function
      // type.
      QualType T = Func->getType();
      QualType NoProtoType = T;
      if (const FunctionProtoType *Proto = T->getAsFunctionProtoType())
        NoProtoType = Context.getFunctionNoProtoType(Proto->getResultType());
      return Owned(BuildDeclRefExpr(VD, NoProtoType, Loc, false, false, SS));
    }
  }

  // Only create DeclRefExpr's for valid Decl's.
  if (VD->isInvalidDecl())
    return ExprError();

  // If the identifier reference is inside a block, and it refers to a value
  // that is outside the block, create a BlockDeclRefExpr instead of a
  // DeclRefExpr.  This ensures the value is treated as a copy-in snapshot when
  // the block is formed.
  //
  // We do not do this for things like enum constants, global variables, etc,
  // as they do not get snapshotted.
  //
  if (CurBlock && ShouldSnapshotBlockValueReference(CurBlock, VD)) {
    // Blocks that have these can't be constant.
    CurBlock->hasBlockDeclRefExprs = true;

    // The BlocksAttr indicates the variable is bound by-reference.
    if (VD->getAttr<BlocksAttr>())
      return Owned(new (Context) BlockDeclRefExpr(VD, 
                               VD->getType().getNonReferenceType(), Loc, true));

    // Variable will be bound by-copy, make it const within the closure.
    VD->getType().addConst();
    return Owned(new (Context) BlockDeclRefExpr(VD, 
                             VD->getType().getNonReferenceType(), Loc, false));
  }
  // If this reference is not in a block or if the referenced variable is
  // within the block, create a normal DeclRefExpr.

  bool TypeDependent = false;
  bool ValueDependent = false;
  if (getLangOptions().CPlusPlus) {
    // C++ [temp.dep.expr]p3:
    //   An id-expression is type-dependent if it contains:   
    //     - an identifier that was declared with a dependent type,
    if (VD->getType()->isDependentType())
      TypeDependent = true;
    //     - FIXME: a template-id that is dependent,
    //     - a conversion-function-id that specifies a dependent type,
    else if (Name.getNameKind() == DeclarationName::CXXConversionFunctionName &&
             Name.getCXXNameType()->isDependentType())
      TypeDependent = true;
    //     - a nested-name-specifier that contains a class-name that
    //       names a dependent type.
    else if (SS && !SS->isEmpty()) {
      for (DeclContext *DC = static_cast<DeclContext*>(SS->getScopeRep()); 
           DC; DC = DC->getParent()) {
        // FIXME: could stop early at namespace scope.
        if (DC->isRecord()) {
          CXXRecordDecl *Record = cast<CXXRecordDecl>(DC);
          if (Context.getTypeDeclType(Record)->isDependentType()) {
            TypeDependent = true;
            break;
          }
        }
      }
    }

    // C++ [temp.dep.constexpr]p2:
    //
    //   An identifier is value-dependent if it is:
    //     - a name declared with a dependent type,
    if (TypeDependent)
      ValueDependent = true;
    //     - the name of a non-type template parameter,
    else if (isa<NonTypeTemplateParmDecl>(VD))
      ValueDependent = true;
    //    - a constant with integral or enumeration type and is
    //      initialized with an expression that is value-dependent
    //      (FIXME!).
  }

  return Owned(BuildDeclRefExpr(VD, VD->getType().getNonReferenceType(), Loc,
                                TypeDependent, ValueDependent, SS));
}

Sema::OwningExprResult Sema::ActOnPredefinedExpr(SourceLocation Loc,
                                                 tok::TokenKind Kind) {
  PredefinedExpr::IdentType IT;

  switch (Kind) {
  default: assert(0 && "Unknown simple primary expr!");
  case tok::kw___func__: IT = PredefinedExpr::Func; break; // [C99 6.4.2.2]
  case tok::kw___FUNCTION__: IT = PredefinedExpr::Function; break;
  case tok::kw___PRETTY_FUNCTION__: IT = PredefinedExpr::PrettyFunction; break;
  }

  // Pre-defined identifiers are of type char[x], where x is the length of the
  // string.
  unsigned Length;
  if (FunctionDecl *FD = getCurFunctionDecl())
    Length = FD->getIdentifier()->getLength();
  else if (ObjCMethodDecl *MD = getCurMethodDecl())
    Length = MD->getSynthesizedMethodSize();
  else {
    Diag(Loc, diag::ext_predef_outside_function);
    // __PRETTY_FUNCTION__ -> "top level", the others produce an empty string.
    Length = IT == PredefinedExpr::PrettyFunction ? strlen("top level") : 0;
  }


  llvm::APInt LengthI(32, Length + 1);
  QualType ResTy = Context.CharTy.getQualifiedType(QualType::Const);
  ResTy = Context.getConstantArrayType(ResTy, LengthI, ArrayType::Normal, 0);
  return Owned(new (Context) PredefinedExpr(Loc, ResTy, IT));
}

Sema::OwningExprResult Sema::ActOnCharacterConstant(const Token &Tok) {
  llvm::SmallString<16> CharBuffer;
  CharBuffer.resize(Tok.getLength());
  const char *ThisTokBegin = &CharBuffer[0];
  unsigned ActualLength = PP.getSpelling(Tok, ThisTokBegin);

  CharLiteralParser Literal(ThisTokBegin, ThisTokBegin+ActualLength,
                            Tok.getLocation(), PP);
  if (Literal.hadError())
    return ExprError();

  QualType type = getLangOptions().CPlusPlus ? Context.CharTy : Context.IntTy;

  return Owned(new (Context) CharacterLiteral(Literal.getValue(),
                                              Literal.isWide(),
                                              type, Tok.getLocation()));
}

Action::OwningExprResult Sema::ActOnNumericConstant(const Token &Tok) {
  // Fast path for a single digit (which is quite common).  A single digit
  // cannot have a trigraph, escaped newline, radix prefix, or type suffix.
  if (Tok.getLength() == 1) {
    const char Val = PP.getSpellingOfSingleCharacterNumericConstant(Tok);
    unsigned IntSize = Context.Target.getIntWidth();
    return Owned(new (Context) IntegerLiteral(llvm::APInt(IntSize, Val-'0'),
                    Context.IntTy, Tok.getLocation()));
  }

  llvm::SmallString<512> IntegerBuffer;
  // Add padding so that NumericLiteralParser can overread by one character.
  IntegerBuffer.resize(Tok.getLength()+1);
  const char *ThisTokBegin = &IntegerBuffer[0];

  // Get the spelling of the token, which eliminates trigraphs, etc.
  unsigned ActualLength = PP.getSpelling(Tok, ThisTokBegin);

  NumericLiteralParser Literal(ThisTokBegin, ThisTokBegin+ActualLength, 
                               Tok.getLocation(), PP);
  if (Literal.hadError)
    return ExprError();

  Expr *Res;

  if (Literal.isFloatingLiteral()) {
    QualType Ty;
    if (Literal.isFloat)
      Ty = Context.FloatTy;
    else if (!Literal.isLong)
      Ty = Context.DoubleTy;
    else
      Ty = Context.LongDoubleTy;

    const llvm::fltSemantics &Format = Context.getFloatTypeSemantics(Ty);

    // isExact will be set by GetFloatValue().
    bool isExact = false;
    Res = new (Context) FloatingLiteral(Literal.GetFloatValue(Format, &isExact),
                                        &isExact, Ty, Tok.getLocation());

  } else if (!Literal.isIntegerLiteral()) {
    return ExprError();
  } else {
    QualType Ty;

    // long long is a C99 feature.
    if (!getLangOptions().C99 && !getLangOptions().CPlusPlus0x &&
        Literal.isLongLong)
      Diag(Tok.getLocation(), diag::ext_longlong);

    // Get the value in the widest-possible width.
    llvm::APInt ResultVal(Context.Target.getIntMaxTWidth(), 0);

    if (Literal.GetIntegerValue(ResultVal)) {
      // If this value didn't fit into uintmax_t, warn and force to ull.
      Diag(Tok.getLocation(), diag::warn_integer_too_large);
      Ty = Context.UnsignedLongLongTy;
      assert(Context.getTypeSize(Ty) == ResultVal.getBitWidth() &&
             "long long is not intmax_t?");
    } else {
      // If this value fits into a ULL, try to figure out what else it fits into
      // according to the rules of C99 6.4.4.1p5.

      // Octal, Hexadecimal, and integers with a U suffix are allowed to
      // be an unsigned int.
      bool AllowUnsigned = Literal.isUnsigned || Literal.getRadix() != 10;

      // Check from smallest to largest, picking the smallest type we can.
      unsigned Width = 0;
      if (!Literal.isLong && !Literal.isLongLong) {
        // Are int/unsigned possibilities?
        unsigned IntSize = Context.Target.getIntWidth();

        // Does it fit in a unsigned int?
        if (ResultVal.isIntN(IntSize)) {
          // Does it fit in a signed int?
          if (!Literal.isUnsigned && ResultVal[IntSize-1] == 0)
            Ty = Context.IntTy;
          else if (AllowUnsigned)
            Ty = Context.UnsignedIntTy;
          Width = IntSize;
        }
      }

      // Are long/unsigned long possibilities?
      if (Ty.isNull() && !Literal.isLongLong) {
        unsigned LongSize = Context.Target.getLongWidth();

        // Does it fit in a unsigned long?
        if (ResultVal.isIntN(LongSize)) {
          // Does it fit in a signed long?
          if (!Literal.isUnsigned && ResultVal[LongSize-1] == 0)
            Ty = Context.LongTy;
          else if (AllowUnsigned)
            Ty = Context.UnsignedLongTy;
          Width = LongSize;
        }
      }

      // Finally, check long long if needed.
      if (Ty.isNull()) {
        unsigned LongLongSize = Context.Target.getLongLongWidth();

        // Does it fit in a unsigned long long?
        if (ResultVal.isIntN(LongLongSize)) {
          // Does it fit in a signed long long?
          if (!Literal.isUnsigned && ResultVal[LongLongSize-1] == 0)
            Ty = Context.LongLongTy;
          else if (AllowUnsigned)
            Ty = Context.UnsignedLongLongTy;
          Width = LongLongSize;
        }
      }

      // If we still couldn't decide a type, we probably have something that
      // does not fit in a signed long long, but has no U suffix.
      if (Ty.isNull()) {
        Diag(Tok.getLocation(), diag::warn_integer_too_large_for_signed);
        Ty = Context.UnsignedLongLongTy;
        Width = Context.Target.getLongLongWidth();
      }

      if (ResultVal.getBitWidth() != Width)
        ResultVal.trunc(Width);
    }
    Res = new (Context) IntegerLiteral(ResultVal, Ty, Tok.getLocation());
  }

  // If this is an imaginary literal, create the ImaginaryLiteral wrapper.
  if (Literal.isImaginary)
    Res = new (Context) ImaginaryLiteral(Res, 
                                        Context.getComplexType(Res->getType()));

  return Owned(Res);
}

Action::OwningExprResult Sema::ActOnParenExpr(SourceLocation L,
                                              SourceLocation R, ExprArg Val) {
  Expr *E = (Expr *)Val.release();
  assert((E != 0) && "ActOnParenExpr() missing expr");
  return Owned(new (Context) ParenExpr(L, R, E));
}

/// The UsualUnaryConversions() function is *not* called by this routine.
/// See C99 6.3.2.1p[2-4] for more details.
bool Sema::CheckSizeOfAlignOfOperand(QualType exprType,
                                     SourceLocation OpLoc,
                                     const SourceRange &ExprRange,
                                     bool isSizeof) {
  if (exprType->isDependentType())
    return false;

  // C99 6.5.3.4p1:
  if (isa<FunctionType>(exprType)) {
    // alignof(function) is allowed.
    if (isSizeof)
      Diag(OpLoc, diag::ext_sizeof_function_type) << ExprRange;
    return false;
  }
  
  if (exprType->isVoidType()) {
    Diag(OpLoc, diag::ext_sizeof_void_type)
      << (isSizeof ? "sizeof" : "__alignof") << ExprRange;
    return false;
  }

  return RequireCompleteType(OpLoc, exprType,
                                isSizeof ? diag::err_sizeof_incomplete_type : 
                                           diag::err_alignof_incomplete_type,
                                ExprRange);
}

bool Sema::CheckAlignOfExpr(Expr *E, SourceLocation OpLoc,
                            const SourceRange &ExprRange) {
  E = E->IgnoreParens();

  // alignof decl is always ok. 
  if (isa<DeclRefExpr>(E))
    return false;

  // Cannot know anything else if the expression is dependent.
  if (E->isTypeDependent())
    return false;

  if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
    if (FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
      if (FD->isBitField()) {
        Diag(OpLoc, diag::err_sizeof_alignof_bitfield) << 1 << ExprRange;
        return true;
      }
      // Other fields are ok.
      return false;
    }
  }
  return CheckSizeOfAlignOfOperand(E->getType(), OpLoc, ExprRange, false);
}

/// ActOnSizeOfAlignOfExpr - Handle @c sizeof(type) and @c sizeof @c expr and
/// the same for @c alignof and @c __alignof
/// Note that the ArgRange is invalid if isType is false.
Action::OwningExprResult
Sema::ActOnSizeOfAlignOfExpr(SourceLocation OpLoc, bool isSizeof, bool isType,
                             void *TyOrEx, const SourceRange &ArgRange) {
  // If error parsing type, ignore.
  if (TyOrEx == 0) return ExprError();

  QualType ArgTy;
  SourceRange Range;
  if (isType) {
    ArgTy = QualType::getFromOpaquePtr(TyOrEx);
    Range = ArgRange;
    
    // Verify that the operand is valid.
    if (CheckSizeOfAlignOfOperand(ArgTy, OpLoc, Range, isSizeof))
      return ExprError();
  } else {
    // Get the end location.
    Expr *ArgEx = (Expr *)TyOrEx;
    Range = ArgEx->getSourceRange();
    ArgTy = ArgEx->getType();
    
    // Verify that the operand is valid.
    bool isInvalid;
    if (!isSizeof) {
      isInvalid = CheckAlignOfExpr(ArgEx, OpLoc, Range);
    } else if (ArgEx->isBitField()) {  // C99 6.5.3.4p1.
      Diag(OpLoc, diag::err_sizeof_alignof_bitfield) << 0;
      isInvalid = true;
    } else {
      isInvalid = CheckSizeOfAlignOfOperand(ArgTy, OpLoc, Range, true);
    }
    
    if (isInvalid) {
      DeleteExpr(ArgEx);
      return ExprError();
    }
  }

  // C99 6.5.3.4p4: the type (an unsigned integer type) is size_t.
  return Owned(new (Context) SizeOfAlignOfExpr(isSizeof, isType, TyOrEx,
                                               Context.getSizeType(), OpLoc,
                                               Range.getEnd()));
}

QualType Sema::CheckRealImagOperand(Expr *&V, SourceLocation Loc, bool isReal) {
  if (V->isTypeDependent())
    return Context.DependentTy;

  DefaultFunctionArrayConversion(V);
  
  // These operators return the element type of a complex type.
  if (const ComplexType *CT = V->getType()->getAsComplexType())
    return CT->getElementType();
  
  // Otherwise they pass through real integer and floating point types here.
  if (V->getType()->isArithmeticType())
    return V->getType();
  
  // Reject anything else.
  Diag(Loc, diag::err_realimag_invalid_type) << V->getType()
    << (isReal ? "__real" : "__imag");
  return QualType();
}



Action::OwningExprResult
Sema::ActOnPostfixUnaryOp(Scope *S, SourceLocation OpLoc,
                          tok::TokenKind Kind, ExprArg Input) {
  Expr *Arg = (Expr *)Input.get();

  UnaryOperator::Opcode Opc;
  switch (Kind) {
  default: assert(0 && "Unknown unary op!");
  case tok::plusplus:   Opc = UnaryOperator::PostInc; break;
  case tok::minusminus: Opc = UnaryOperator::PostDec; break;
  }

  if (getLangOptions().CPlusPlus &&
      (Arg->getType()->isRecordType() || Arg->getType()->isEnumeralType())) {
    // Which overloaded operator?
    OverloadedOperatorKind OverOp =
      (Opc == UnaryOperator::PostInc)? OO_PlusPlus : OO_MinusMinus;

    // C++ [over.inc]p1:
    //
    //     [...] If the function is a member function with one
    //     parameter (which shall be of type int) or a non-member
    //     function with two parameters (the second of which shall be
    //     of type int), it defines the postfix increment operator ++
    //     for objects of that type. When the postfix increment is
    //     called as a result of using the ++ operator, the int
    //     argument will have value zero.
    Expr *Args[2] = { 
      Arg, 
      new (Context) IntegerLiteral(llvm::APInt(Context.Target.getIntWidth(), 0, 
                          /*isSigned=*/true), Context.IntTy, SourceLocation())
    };

    // Build the candidate set for overloading
    OverloadCandidateSet CandidateSet;
    if (AddOperatorCandidates(OverOp, S, OpLoc, Args, 2, CandidateSet))
      return ExprError();

    // Perform overload resolution.
    OverloadCandidateSet::iterator Best;
    switch (BestViableFunction(CandidateSet, Best)) {
    case OR_Success: {
      // We found a built-in operator or an overloaded operator.
      FunctionDecl *FnDecl = Best->Function;

      if (FnDecl) {
        // We matched an overloaded operator. Build a call to that
        // operator.

        // Convert the arguments.
        if (CXXMethodDecl *Method = dyn_cast<CXXMethodDecl>(FnDecl)) {
          if (PerformObjectArgumentInitialization(Arg, Method))
            return ExprError();
        } else {
          // Convert the arguments.
          if (PerformCopyInitialization(Arg,
                                        FnDecl->getParamDecl(0)->getType(),
                                        "passing"))
            return ExprError();
        }

        // Determine the result type
        QualType ResultTy
          = FnDecl->getType()->getAsFunctionType()->getResultType();
        ResultTy = ResultTy.getNonReferenceType();

        // Build the actual expression node.
        Expr *FnExpr = new (Context) DeclRefExpr(FnDecl, FnDecl->getType(),
                                                 SourceLocation());
        UsualUnaryConversions(FnExpr);

        Input.release();
        return Owned(new (Context) CXXOperatorCallExpr(Context, FnExpr, Args, 2,
                                                       ResultTy, OpLoc));
      } else {
        // We matched a built-in operator. Convert the arguments, then
        // break out so that we will build the appropriate built-in
        // operator node.
        if (PerformCopyInitialization(Arg, Best->BuiltinTypes.ParamTypes[0],
                                      "passing"))
          return ExprError();

        break;
      }
    }

    case OR_No_Viable_Function:
      // No viable function; fall through to handling this as a
      // built-in operator, which will produce an error message for us.
      break;

    case OR_Ambiguous:
      Diag(OpLoc,  diag::err_ovl_ambiguous_oper)
          << UnaryOperator::getOpcodeStr(Opc)
          << Arg->getSourceRange();
      PrintOverloadCandidates(CandidateSet, /*OnlyViable=*/true);
      return ExprError();

    case OR_Deleted:
      Diag(OpLoc, diag::err_ovl_deleted_oper)
        << Best->Function->isDeleted()
        << UnaryOperator::getOpcodeStr(Opc)
        << Arg->getSourceRange();
      PrintOverloadCandidates(CandidateSet, /*OnlyViable=*/true);
      return ExprError();
    }

    // Either we found no viable overloaded operator or we matched a
    // built-in operator. In either case, fall through to trying to
    // build a built-in operation.
  }

  QualType result = CheckIncrementDecrementOperand(Arg, OpLoc,
                                                 Opc == UnaryOperator::PostInc);
  if (result.isNull())
    return ExprError();
  Input.release();
  return Owned(new (Context) UnaryOperator(Arg, Opc, result, OpLoc));
}

Action::OwningExprResult
Sema::ActOnArraySubscriptExpr(Scope *S, ExprArg Base, SourceLocation LLoc,
                              ExprArg Idx, SourceLocation RLoc) {
  Expr *LHSExp = static_cast<Expr*>(Base.get()),
       *RHSExp = static_cast<Expr*>(Idx.get());

  if (getLangOptions().CPlusPlus &&
      (LHSExp->getType()->isRecordType() ||
       LHSExp->getType()->isEnumeralType() ||
       RHSExp->getType()->isRecordType() ||
       RHSExp->getType()->isEnumeralType())) {
    // Add the appropriate overloaded operators (C++ [over.match.oper]) 
    // to the candidate set.
    OverloadCandidateSet CandidateSet;
    Expr *Args[2] = { LHSExp, RHSExp };
    if (AddOperatorCandidates(OO_Subscript, S, LLoc, Args, 2, CandidateSet,
                              SourceRange(LLoc, RLoc)))
      return ExprError();

    // Perform overload resolution.
    OverloadCandidateSet::iterator Best;
    switch (BestViableFunction(CandidateSet, Best)) {
    case OR_Success: {
      // We found a built-in operator or an overloaded operator.
      FunctionDecl *FnDecl = Best->Function;

      if (FnDecl) {
        // We matched an overloaded operator. Build a call to that
        // operator.

        // Convert the arguments.
        if (CXXMethodDecl *Method = dyn_cast<CXXMethodDecl>(FnDecl)) {
          if (PerformObjectArgumentInitialization(LHSExp, Method) ||
              PerformCopyInitialization(RHSExp, 
                                        FnDecl->getParamDecl(0)->getType(),
                                        "passing"))
            return ExprError();
        } else {
          // Convert the arguments.
          if (PerformCopyInitialization(LHSExp,
                                        FnDecl->getParamDecl(0)->getType(),
                                        "passing") ||
              PerformCopyInitialization(RHSExp,
                                        FnDecl->getParamDecl(1)->getType(),
                                        "passing"))
            return ExprError();
        }

        // Determine the result type
        QualType ResultTy
          = FnDecl->getType()->getAsFunctionType()->getResultType();
        ResultTy = ResultTy.getNonReferenceType();

        // Build the actual expression node.
        Expr *FnExpr = new (Context) DeclRefExpr(FnDecl, FnDecl->getType(),
                                                 SourceLocation());
        UsualUnaryConversions(FnExpr);

        Base.release();
        Idx.release();
        return Owned(new (Context) CXXOperatorCallExpr(Context, FnExpr, Args, 2,
                                                       ResultTy, LLoc));
      } else {
        // We matched a built-in operator. Convert the arguments, then
        // break out so that we will build the appropriate built-in
        // operator node.
        if (PerformCopyInitialization(LHSExp, Best->BuiltinTypes.ParamTypes[0],
                                      "passing") ||
            PerformCopyInitialization(RHSExp, Best->BuiltinTypes.ParamTypes[1],
                                      "passing"))
          return ExprError();

        break;
      }
    }

    case OR_No_Viable_Function:
      // No viable function; fall through to handling this as a
      // built-in operator, which will produce an error message for us.
      break;

    case OR_Ambiguous:
      Diag(LLoc,  diag::err_ovl_ambiguous_oper)
          << "[]"
          << LHSExp->getSourceRange() << RHSExp->getSourceRange();
      PrintOverloadCandidates(CandidateSet, /*OnlyViable=*/true);
      return ExprError();

    case OR_Deleted:
      Diag(LLoc, diag::err_ovl_deleted_oper)
        << Best->Function->isDeleted()
        << "[]"
        << LHSExp->getSourceRange() << RHSExp->getSourceRange();
      PrintOverloadCandidates(CandidateSet, /*OnlyViable=*/true);
      return ExprError();
    }

    // Either we found no viable overloaded operator or we matched a
    // built-in operator. In either case, fall through to trying to
    // build a built-in operation.
  }

  // Perform default conversions.
  DefaultFunctionArrayConversion(LHSExp);
  DefaultFunctionArrayConversion(RHSExp);

  QualType LHSTy = LHSExp->getType(), RHSTy = RHSExp->getType();

  // C99 6.5.2.1p2: the expression e1[e2] is by definition precisely equivalent
  // to the expression *((e1)+(e2)). This means the array "Base" may actually be
  // in the subscript position. As a result, we need to derive the array base
  // and index from the expression types.
  Expr *BaseExpr, *IndexExpr;
  QualType ResultType;
  if (LHSTy->isDependentType() || RHSTy->isDependentType()) {
    BaseExpr = LHSExp;
    IndexExpr = RHSExp;
    ResultType = Context.DependentTy;
  } else if (const PointerType *PTy = LHSTy->getAsPointerType()) {
    BaseExpr = LHSExp;
    IndexExpr = RHSExp;
    // FIXME: need to deal with const...
    ResultType = PTy->getPointeeType();
  } else if (const PointerType *PTy = RHSTy->getAsPointerType()) {
     // Handle the uncommon case of "123[Ptr]".
    BaseExpr = RHSExp;
    IndexExpr = LHSExp;
    // FIXME: need to deal with const...
    ResultType = PTy->getPointeeType();
  } else if (const VectorType *VTy = LHSTy->getAsVectorType()) {
    BaseExpr = LHSExp;    // vectors: V[123]
    IndexExpr = RHSExp;

    // FIXME: need to deal with const...
    ResultType = VTy->getElementType();
  } else {
    return ExprError(Diag(LHSExp->getLocStart(),
      diag::err_typecheck_subscript_value) << RHSExp->getSourceRange());
  }
  // C99 6.5.2.1p1
  if (!IndexExpr->getType()->isIntegerType() && !IndexExpr->isTypeDependent())
    return ExprError(Diag(IndexExpr->getLocStart(),
      diag::err_typecheck_subscript) << IndexExpr->getSourceRange());

  // C99 6.5.2.1p1: "shall have type "pointer to *object* type".  In practice,
  // the following check catches trying to index a pointer to a function (e.g.
  // void (*)(int)) and pointers to incomplete types.  Functions are not
  // objects in C99.
  if (!ResultType->isObjectType() && !ResultType->isDependentType())
    return ExprError(Diag(BaseExpr->getLocStart(),
                diag::err_typecheck_subscript_not_object)
      << BaseExpr->getType() << BaseExpr->getSourceRange());

  Base.release();
  Idx.release();
  return Owned(new (Context) ArraySubscriptExpr(LHSExp, RHSExp,
                                                ResultType, RLoc));
}

QualType Sema::
CheckExtVectorComponent(QualType baseType, SourceLocation OpLoc,
                        IdentifierInfo &CompName, SourceLocation CompLoc) {
  const ExtVectorType *vecType = baseType->getAsExtVectorType();

  // The vector accessor can't exceed the number of elements.
  const char *compStr = CompName.getName();

  // This flag determines whether or not the component is one of the four
  // special names that indicate a subset of exactly half the elements are
  // to be selected.
  bool HalvingSwizzle = false;

  // This flag determines whether or not CompName has an 's' char prefix,
  // indicating that it is a string of hex values to be used as vector indices.
  bool HexSwizzle = *compStr == 's';

  // Check that we've found one of the special components, or that the component
  // names must come from the same set.
  if (!strcmp(compStr, "hi") || !strcmp(compStr, "lo") ||
      !strcmp(compStr, "even") || !strcmp(compStr, "odd")) {
    HalvingSwizzle = true;
  } else if (vecType->getPointAccessorIdx(*compStr) != -1) {
    do
      compStr++;
    while (*compStr && vecType->getPointAccessorIdx(*compStr) != -1);
  } else if (HexSwizzle || vecType->getNumericAccessorIdx(*compStr) != -1) {
    do
      compStr++;
    while (*compStr && vecType->getNumericAccessorIdx(*compStr) != -1);
  }

  if (!HalvingSwizzle && *compStr) {
    // We didn't get to the end of the string. This means the component names
    // didn't come from the same set *or* we encountered an illegal name.
    Diag(OpLoc, diag::err_ext_vector_component_name_illegal)
      << std::string(compStr,compStr+1) << SourceRange(CompLoc);
    return QualType();
  }

  // Ensure no component accessor exceeds the width of the vector type it
  // operates on.
  if (!HalvingSwizzle) {
    compStr = CompName.getName();

    if (HexSwizzle)
      compStr++;

    while (*compStr) {
      if (!vecType->isAccessorWithinNumElements(*compStr++)) {
        Diag(OpLoc, diag::err_ext_vector_component_exceeds_length)
          << baseType << SourceRange(CompLoc);
        return QualType();
      }
    }
  }

  // If this is a halving swizzle, verify that the base type has an even
  // number of elements.
  if (HalvingSwizzle && (vecType->getNumElements() & 1U)) {
    Diag(OpLoc, diag::err_ext_vector_component_requires_even)
      << baseType << SourceRange(CompLoc);
    return QualType();
  }

  // The component accessor looks fine - now we need to compute the actual type.
  // The vector type is implied by the component accessor. For example,
  // vec4.b is a float, vec4.xy is a vec2, vec4.rgb is a vec3, etc.
  // vec4.s0 is a float, vec4.s23 is a vec3, etc.
  // vec4.hi, vec4.lo, vec4.e, and vec4.o all return vec2.
  unsigned CompSize = HalvingSwizzle ? vecType->getNumElements() / 2
                                     : CompName.getLength();
  if (HexSwizzle)
    CompSize--;

  if (CompSize == 1)
    return vecType->getElementType();

  QualType VT = Context.getExtVectorType(vecType->getElementType(), CompSize);
  // Now look up the TypeDefDecl from the vector type. Without this,
  // diagostics look bad. We want extended vector types to appear built-in.
  for (unsigned i = 0, E = ExtVectorDecls.size(); i != E; ++i) {
    if (ExtVectorDecls[i]->getUnderlyingType() == VT)
      return Context.getTypedefType(ExtVectorDecls[i]);
  }
  return VT; // should never get here (a typedef type should always be found).
}


Action::OwningExprResult
Sema::ActOnMemberReferenceExpr(Scope *S, ExprArg Base, SourceLocation OpLoc,
                               tok::TokenKind OpKind, SourceLocation MemberLoc,
                               IdentifierInfo &Member,
                               DeclTy *ObjCImpDecl) {
  Expr *BaseExpr = static_cast<Expr *>(Base.release());
  assert(BaseExpr && "no record expression");

  // Perform default conversions.
  DefaultFunctionArrayConversion(BaseExpr);

  QualType BaseType = BaseExpr->getType();
  assert(!BaseType.isNull() && "no type for member expression");

  // Get the type being accessed in BaseType.  If this is an arrow, the BaseExpr
  // must have pointer type, and the accessed type is the pointee.
  if (OpKind == tok::arrow) {
    if (const PointerType *PT = BaseType->getAsPointerType())
      BaseType = PT->getPointeeType();
    else if (getLangOptions().CPlusPlus && BaseType->isRecordType())
      return Owned(BuildOverloadedArrowExpr(S, BaseExpr, OpLoc,
                                            MemberLoc, Member));
    else
      return ExprError(Diag(MemberLoc,
                            diag::err_typecheck_member_reference_arrow)
        << BaseType << BaseExpr->getSourceRange());
  }

  // Handle field access to simple records.  This also handles access to fields
  // of the ObjC 'id' struct.
  if (const RecordType *RTy = BaseType->getAsRecordType()) {
    RecordDecl *RDecl = RTy->getDecl();
    if (RequireCompleteType(OpLoc, BaseType,
                               diag::err_typecheck_incomplete_tag,
                               BaseExpr->getSourceRange()))
      return ExprError();

    // The record definition is complete, now make sure the member is valid.
    // FIXME: Qualified name lookup for C++ is a bit more complicated
    // than this.
    LookupResult Result
      = LookupQualifiedName(RDecl, DeclarationName(&Member),
                            LookupMemberName, false);

    NamedDecl *MemberDecl = 0;
    if (!Result)
      return ExprError(Diag(MemberLoc, diag::err_typecheck_no_member)
               << &Member << BaseExpr->getSourceRange());
    else if (Result.isAmbiguous()) {
      DiagnoseAmbiguousLookup(Result, DeclarationName(&Member),
                              MemberLoc, BaseExpr->getSourceRange());
      return ExprError();
    } else
      MemberDecl = Result;

    // If the decl being referenced had an error, return an error for this
    // sub-expr without emitting another error, in order to avoid cascading
    // error cases.
    if (MemberDecl->isInvalidDecl())
      return ExprError();

    // Check the use of this field
    if (DiagnoseUseOfDecl(MemberDecl, MemberLoc))
      return ExprError();

    if (FieldDecl *FD = dyn_cast<FieldDecl>(MemberDecl)) {
      // We may have found a field within an anonymous union or struct
      // (C++ [class.union]).
      if (cast<RecordDecl>(FD->getDeclContext())->isAnonymousStructOrUnion())
        return BuildAnonymousStructUnionMemberReference(MemberLoc, FD,
                                                        BaseExpr, OpLoc);

      // Figure out the type of the member; see C99 6.5.2.3p3, C++ [expr.ref]
      // FIXME: Handle address space modifiers
      QualType MemberType = FD->getType();
      if (const ReferenceType *Ref = MemberType->getAsReferenceType())
        MemberType = Ref->getPointeeType();
      else {
        unsigned combinedQualifiers =
          MemberType.getCVRQualifiers() | BaseType.getCVRQualifiers();
        if (FD->isMutable())
          combinedQualifiers &= ~QualType::Const;
        MemberType = MemberType.getQualifiedType(combinedQualifiers);
      }

      return Owned(new (Context) MemberExpr(BaseExpr, OpKind == tok::arrow, FD,
                                            MemberLoc, MemberType));
    } else if (CXXClassVarDecl *Var = dyn_cast<CXXClassVarDecl>(MemberDecl))
      return Owned(new (Context) MemberExpr(BaseExpr, OpKind == tok::arrow,
                                  Var, MemberLoc,
                                  Var->getType().getNonReferenceType()));
    else if (FunctionDecl *MemberFn = dyn_cast<FunctionDecl>(MemberDecl))
      return Owned(new (Context) MemberExpr(BaseExpr, OpKind == tok::arrow,
                                  MemberFn, MemberLoc, MemberFn->getType()));
    else if (OverloadedFunctionDecl *Ovl
             = dyn_cast<OverloadedFunctionDecl>(MemberDecl))
      return Owned(new (Context) MemberExpr(BaseExpr, OpKind == tok::arrow, Ovl,
                                  MemberLoc, Context.OverloadTy));
    else if (EnumConstantDecl *Enum = dyn_cast<EnumConstantDecl>(MemberDecl))
      return Owned(new (Context) MemberExpr(BaseExpr, OpKind == tok::arrow,
                                            Enum, MemberLoc, Enum->getType()));
    else if (isa<TypeDecl>(MemberDecl))
      return ExprError(Diag(MemberLoc,diag::err_typecheck_member_reference_type)
        << DeclarationName(&Member) << int(OpKind == tok::arrow));

    // We found a declaration kind that we didn't expect. This is a
    // generic error message that tells the user that she can't refer
    // to this member with '.' or '->'.
    return ExprError(Diag(MemberLoc,
                          diag::err_typecheck_member_reference_unknown)
      << DeclarationName(&Member) << int(OpKind == tok::arrow));
  }

  // Handle access to Objective-C instance variables, such as "Obj->ivar" and
  // (*Obj).ivar.
  if (const ObjCInterfaceType *IFTy = BaseType->getAsObjCInterfaceType()) {
    ObjCInterfaceDecl *ClassDeclared;
    if (ObjCIvarDecl *IV = IFTy->getDecl()->lookupInstanceVariable(&Member, 
                                                             ClassDeclared)) {
      // If the decl being referenced had an error, return an error for this
      // sub-expr without emitting another error, in order to avoid cascading
      // error cases.
      if (IV->isInvalidDecl())
        return ExprError();

      // Check whether we can reference this field.
      if (DiagnoseUseOfDecl(IV, MemberLoc))
        return ExprError();
      if (IV->getAccessControl() != ObjCIvarDecl::Public) {
        ObjCInterfaceDecl *ClassOfMethodDecl = 0;
        if (ObjCMethodDecl *MD = getCurMethodDecl())
          ClassOfMethodDecl =  MD->getClassInterface();
        else if (ObjCImpDecl && getCurFunctionDecl()) {
          // Case of a c-function declared inside an objc implementation.
          // FIXME: For a c-style function nested inside an objc implementation
          // class, there is no implementation context available, so we pass down
          // the context as argument to this routine. Ideally, this context need
          // be passed down in the AST node and somehow calculated from the AST
          // for a function decl.
          Decl *ImplDecl = static_cast<Decl *>(ObjCImpDecl);
          if (ObjCImplementationDecl *IMPD = 
              dyn_cast<ObjCImplementationDecl>(ImplDecl))
            ClassOfMethodDecl = IMPD->getClassInterface();
          else if (ObjCCategoryImplDecl* CatImplClass =
                      dyn_cast<ObjCCategoryImplDecl>(ImplDecl))
            ClassOfMethodDecl = CatImplClass->getClassInterface();
        }
        if (IV->getAccessControl() == ObjCIvarDecl::Private) { 
          if (ClassDeclared != IFTy->getDecl() || 
              ClassOfMethodDecl != ClassDeclared)
            Diag(MemberLoc, diag::error_private_ivar_access) << IV->getDeclName();
        }
        // @protected
        else if (!IFTy->getDecl()->isSuperClassOf(ClassOfMethodDecl))
          Diag(MemberLoc, diag::error_protected_ivar_access) << IV->getDeclName();
      }

      ObjCIvarRefExpr *MRef= new (Context) ObjCIvarRefExpr(IV, IV->getType(),
                                                 MemberLoc, BaseExpr,
                                                 OpKind == tok::arrow);
      Context.setFieldDecl(IFTy->getDecl(), IV, MRef);
      return Owned(MRef);
    }
    return ExprError(Diag(MemberLoc, diag::err_typecheck_member_reference_ivar)
                       << IFTy->getDecl()->getDeclName() << &Member
                       << BaseExpr->getSourceRange());
  }

  // Handle Objective-C property access, which is "Obj.property" where Obj is a
  // pointer to a (potentially qualified) interface type.
  const PointerType *PTy;
  const ObjCInterfaceType *IFTy;
  if (OpKind == tok::period && (PTy = BaseType->getAsPointerType()) &&
      (IFTy = PTy->getPointeeType()->getAsObjCInterfaceType())) {
    ObjCInterfaceDecl *IFace = IFTy->getDecl();

    // Search for a declared property first.
    if (ObjCPropertyDecl *PD = IFace->FindPropertyDeclaration(&Member)) {
      // Check whether we can reference this property.
      if (DiagnoseUseOfDecl(PD, MemberLoc))
        return ExprError();

      return Owned(new (Context) ObjCPropertyRefExpr(PD, PD->getType(),
                                                     MemberLoc, BaseExpr));
    }

    // Check protocols on qualified interfaces.
    for (ObjCInterfaceType::qual_iterator I = IFTy->qual_begin(),
         E = IFTy->qual_end(); I != E; ++I)
      if (ObjCPropertyDecl *PD = (*I)->FindPropertyDeclaration(&Member)) {
        // Check whether we can reference this property.
        if (DiagnoseUseOfDecl(PD, MemberLoc))
          return ExprError();

        return Owned(new (Context) ObjCPropertyRefExpr(PD, PD->getType(),
                                                       MemberLoc, BaseExpr));
      }

    // If that failed, look for an "implicit" property by seeing if the nullary
    // selector is implemented.

    // FIXME: The logic for looking up nullary and unary selectors should be
    // shared with the code in ActOnInstanceMessage.

    Selector Sel = PP.getSelectorTable().getNullarySelector(&Member);
    ObjCMethodDecl *Getter = IFace->lookupInstanceMethod(Sel);

    // If this reference is in an @implementation, check for 'private' methods.
    if (!Getter)
      if (ObjCImplementationDecl *ImpDecl =
          ObjCImplementations[IFace->getIdentifier()])
        Getter = ImpDecl->getInstanceMethod(Sel);

    // Look through local category implementations associated with the class.
    if (!Getter) {
      for (unsigned i = 0; i < ObjCCategoryImpls.size() && !Getter; i++) {
        if (ObjCCategoryImpls[i]->getClassInterface() == IFace)
          Getter = ObjCCategoryImpls[i]->getInstanceMethod(Sel);
      }
    }
    if (Getter) {
      // Check if we can reference this property.
      if (DiagnoseUseOfDecl(Getter, MemberLoc))
        return ExprError();
    }
    // If we found a getter then this may be a valid dot-reference, we
    // will look for the matching setter, in case it is needed.
    Selector SetterSel = 
      SelectorTable::constructSetterName(PP.getIdentifierTable(), 
                                         PP.getSelectorTable(), &Member);
    ObjCMethodDecl *Setter = IFace->lookupInstanceMethod(SetterSel);
    if (!Setter) {
      // If this reference is in an @implementation, also check for 'private'
      // methods.
      if (ObjCImplementationDecl *ImpDecl =
          ObjCImplementations[IFace->getIdentifier()])
        Setter = ImpDecl->getInstanceMethod(SetterSel);
    }
    // Look through local category implementations associated with the class.
    if (!Setter) {
      for (unsigned i = 0; i < ObjCCategoryImpls.size() && !Setter; i++) {
        if (ObjCCategoryImpls[i]->getClassInterface() == IFace)
          Setter = ObjCCategoryImpls[i]->getInstanceMethod(SetterSel);
      }
    }

    if (Setter && DiagnoseUseOfDecl(Setter, MemberLoc))
      return ExprError();

    if (Getter || Setter) {
      QualType PType;

      if (Getter)
        PType = Getter->getResultType();
      else {
        for (ObjCMethodDecl::param_iterator PI = Setter->param_begin(),
             E = Setter->param_end(); PI != E; ++PI)
          PType = (*PI)->getType();
      }
      // FIXME: we must check that the setter has property type.
      return Owned(new (Context) ObjCKVCRefExpr(Getter, PType,
                                      Setter, MemberLoc, BaseExpr));
    }
    return ExprError(Diag(MemberLoc, diag::err_property_not_found)
      << &Member << BaseType);
  }
  // Handle properties on qualified "id" protocols.
  const ObjCQualifiedIdType *QIdTy;
  if (OpKind == tok::period && (QIdTy = BaseType->getAsObjCQualifiedIdType())) {
    // Check protocols on qualified interfaces.
    for (ObjCQualifiedIdType::qual_iterator I = QIdTy->qual_begin(),
         E = QIdTy->qual_end(); I != E; ++I) {
      if (ObjCPropertyDecl *PD = (*I)->FindPropertyDeclaration(&Member)) {
        // Check the use of this declaration
        if (DiagnoseUseOfDecl(PD, MemberLoc))
          return ExprError();

        return Owned(new (Context) ObjCPropertyRefExpr(PD, PD->getType(),
                                                       MemberLoc, BaseExpr));
      }
      // Also must look for a getter name which uses property syntax.
      Selector Sel = PP.getSelectorTable().getNullarySelector(&Member);
      if (ObjCMethodDecl *OMD = (*I)->getInstanceMethod(Sel)) {
        // Check the use of this method.
        if (DiagnoseUseOfDecl(OMD, MemberLoc))
          return ExprError();

        return Owned(new (Context) ObjCMessageExpr(BaseExpr, Sel,
                        OMD->getResultType(), OMD, OpLoc, MemberLoc, NULL, 0));
      }
    }

    return ExprError(Diag(MemberLoc, diag::err_property_not_found)
                       << &Member << BaseType);
  }
  // Handle properties on ObjC 'Class' types.
  if (OpKind == tok::period && (BaseType == Context.getObjCClassType())) {
    // Also must look for a getter name which uses property syntax.
    Selector Sel = PP.getSelectorTable().getNullarySelector(&Member);
    if (ObjCMethodDecl *MD = getCurMethodDecl()) {
      ObjCInterfaceDecl *IFace = MD->getClassInterface();
      ObjCMethodDecl *Getter;
      // FIXME: need to also look locally in the implementation.
      if ((Getter = IFace->lookupClassMethod(Sel))) {
        // Check the use of this method.
        if (DiagnoseUseOfDecl(Getter, MemberLoc))
          return ExprError();
      }
      // If we found a getter then this may be a valid dot-reference, we
      // will look for the matching setter, in case it is needed.
      Selector SetterSel = 
        SelectorTable::constructSetterName(PP.getIdentifierTable(), 
                                           PP.getSelectorTable(), &Member);
      ObjCMethodDecl *Setter = IFace->lookupClassMethod(SetterSel);
      if (!Setter) {
        // If this reference is in an @implementation, also check for 'private'
        // methods.
        if (ObjCImplementationDecl *ImpDecl =
            ObjCImplementations[IFace->getIdentifier()])
          Setter = ImpDecl->getInstanceMethod(SetterSel);
      }
      // Look through local category implementations associated with the class.
      if (!Setter) {
        for (unsigned i = 0; i < ObjCCategoryImpls.size() && !Setter; i++) {
          if (ObjCCategoryImpls[i]->getClassInterface() == IFace)
            Setter = ObjCCategoryImpls[i]->getClassMethod(SetterSel);
        }
      }

      if (Setter && DiagnoseUseOfDecl(Setter, MemberLoc))
        return ExprError();

      if (Getter || Setter) {
        QualType PType;

        if (Getter)
          PType = Getter->getResultType();
        else {
          for (ObjCMethodDecl::param_iterator PI = Setter->param_begin(),
               E = Setter->param_end(); PI != E; ++PI)
            PType = (*PI)->getType();
        }
        // FIXME: we must check that the setter has property type.
        return Owned(new (Context) ObjCKVCRefExpr(Getter, PType,
                                        Setter, MemberLoc, BaseExpr));
      }
      return ExprError(Diag(MemberLoc, diag::err_property_not_found)
        << &Member << BaseType);
    }
  }
  
  // Handle 'field access' to vectors, such as 'V.xx'.
  if (BaseType->isExtVectorType()) {
    QualType ret = CheckExtVectorComponent(BaseType, OpLoc, Member, MemberLoc);
    if (ret.isNull())
      return ExprError();
    return Owned(new (Context) ExtVectorElementExpr(ret, BaseExpr, Member,
                                                    MemberLoc));
  }

  return ExprError(Diag(MemberLoc,
                        diag::err_typecheck_member_reference_struct_union)
                     << BaseType << BaseExpr->getSourceRange());
}

/// ConvertArgumentsForCall - Converts the arguments specified in
/// Args/NumArgs to the parameter types of the function FDecl with
/// function prototype Proto. Call is the call expression itself, and
/// Fn is the function expression. For a C++ member function, this
/// routine does not attempt to convert the object argument. Returns
/// true if the call is ill-formed.
bool
Sema::ConvertArgumentsForCall(CallExpr *Call, Expr *Fn,
                              FunctionDecl *FDecl,
                              const FunctionProtoType *Proto,
                              Expr **Args, unsigned NumArgs,
                              SourceLocation RParenLoc) {
  // C99 6.5.2.2p7 - the arguments are implicitly converted, as if by
  // assignment, to the types of the corresponding parameter, ...
  unsigned NumArgsInProto = Proto->getNumArgs();
  unsigned NumArgsToCheck = NumArgs;
  bool Invalid = false;

  // If too few arguments are available (and we don't have default
  // arguments for the remaining parameters), don't make the call.
  if (NumArgs < NumArgsInProto) {
    if (!FDecl || NumArgs < FDecl->getMinRequiredArguments())
      return Diag(RParenLoc, diag::err_typecheck_call_too_few_args)
        << Fn->getType()->isBlockPointerType() << Fn->getSourceRange();
    // Use default arguments for missing arguments
    NumArgsToCheck = NumArgsInProto;
    Call->setNumArgs(Context, NumArgsInProto);
  }

  // If too many are passed and not variadic, error on the extras and drop
  // them.
  if (NumArgs > NumArgsInProto) {
    if (!Proto->isVariadic()) {
      Diag(Args[NumArgsInProto]->getLocStart(),
           diag::err_typecheck_call_too_many_args)
        << Fn->getType()->isBlockPointerType() << Fn->getSourceRange()
        << SourceRange(Args[NumArgsInProto]->getLocStart(),
                       Args[NumArgs-1]->getLocEnd());
      // This deletes the extra arguments.
      Call->setNumArgs(Context, NumArgsInProto);
      Invalid = true;
    }
    NumArgsToCheck = NumArgsInProto;
  }

  // Continue to check argument types (even if we have too few/many args).
  for (unsigned i = 0; i != NumArgsToCheck; i++) {
    QualType ProtoArgType = Proto->getArgType(i);

    Expr *Arg;
    if (i < NumArgs) {
      Arg = Args[i];

      // Pass the argument.
      if (PerformCopyInitialization(Arg, ProtoArgType, "passing"))
        return true;
    } else
      // We already type-checked the argument, so we know it works.
      Arg = new (Context) CXXDefaultArgExpr(FDecl->getParamDecl(i));
    QualType ArgType = Arg->getType();

    Call->setArg(i, Arg);
  }

  // If this is a variadic call, handle args passed through "...".
  if (Proto->isVariadic()) {
    VariadicCallType CallType = VariadicFunction;
    if (Fn->getType()->isBlockPointerType())
      CallType = VariadicBlock; // Block
    else if (isa<MemberExpr>(Fn))
      CallType = VariadicMethod;

    // Promote the arguments (C99 6.5.2.2p7).
    for (unsigned i = NumArgsInProto; i != NumArgs; i++) {
      Expr *Arg = Args[i];
      DefaultVariadicArgumentPromotion(Arg, CallType);
      Call->setArg(i, Arg);
    }
  }

  return Invalid;
}

/// ActOnCallExpr - Handle a call to Fn with the specified array of arguments.
/// This provides the location of the left/right parens and a list of comma
/// locations.
Action::OwningExprResult
Sema::ActOnCallExpr(Scope *S, ExprArg fn, SourceLocation LParenLoc,
                    MultiExprArg args,
                    SourceLocation *CommaLocs, SourceLocation RParenLoc) {
  unsigned NumArgs = args.size();
  Expr *Fn = static_cast<Expr *>(fn.release());
  Expr **Args = reinterpret_cast<Expr**>(args.release());
  assert(Fn && "no function call expression");
  FunctionDecl *FDecl = NULL;
  DeclarationName UnqualifiedName;

  if (getLangOptions().CPlusPlus) {
    // Determine whether this is a dependent call inside a C++ template,
    // in which case we won't do any semantic analysis now.
    // FIXME: Will need to cache the results of name lookup (including ADL) in Fn.
    bool Dependent = false;
    if (Fn->isTypeDependent())
      Dependent = true;
    else if (Expr::hasAnyTypeDependentArguments(Args, NumArgs))
      Dependent = true;

    if (Dependent)
      return Owned(new (Context) CallExpr(Context, Fn, Args, NumArgs,
                                          Context.DependentTy, RParenLoc));

    // Determine whether this is a call to an object (C++ [over.call.object]).
    if (Fn->getType()->isRecordType())
      return Owned(BuildCallToObjectOfClassType(S, Fn, LParenLoc, Args, NumArgs,
                                                CommaLocs, RParenLoc));

    // Determine whether this is a call to a member function.
    if (MemberExpr *MemExpr = dyn_cast<MemberExpr>(Fn->IgnoreParens()))
      if (isa<OverloadedFunctionDecl>(MemExpr->getMemberDecl()) ||
          isa<CXXMethodDecl>(MemExpr->getMemberDecl()))
        return Owned(BuildCallToMemberFunction(S, Fn, LParenLoc, Args, NumArgs,
                                               CommaLocs, RParenLoc));
  }

  // If we're directly calling a function, get the appropriate declaration.
  DeclRefExpr *DRExpr = NULL;
  Expr *FnExpr = Fn;
  bool ADL = true;
  while (true) {
    if (ImplicitCastExpr *IcExpr = dyn_cast<ImplicitCastExpr>(FnExpr))
      FnExpr = IcExpr->getSubExpr();
    else if (ParenExpr *PExpr = dyn_cast<ParenExpr>(FnExpr)) {
      // Parentheses around a function disable ADL
      // (C++0x [basic.lookup.argdep]p1).
      ADL = false;
      FnExpr = PExpr->getSubExpr();
    } else if (isa<UnaryOperator>(FnExpr) &&
               cast<UnaryOperator>(FnExpr)->getOpcode()
                 == UnaryOperator::AddrOf) {
      FnExpr = cast<UnaryOperator>(FnExpr)->getSubExpr();
    } else if ((DRExpr = dyn_cast<DeclRefExpr>(FnExpr))) {
      // Qualified names disable ADL (C++0x [basic.lookup.argdep]p1).
      ADL &= !isa<QualifiedDeclRefExpr>(DRExpr);
      break;
    } else if (UnresolvedFunctionNameExpr *DepName
                 = dyn_cast<UnresolvedFunctionNameExpr>(FnExpr)) {
      UnqualifiedName = DepName->getName();
      break;
    } else {
      // Any kind of name that does not refer to a declaration (or
      // set of declarations) disables ADL (C++0x [basic.lookup.argdep]p3).
      ADL = false;
      break;
    }
  }

  OverloadedFunctionDecl *Ovl = 0;
  if (DRExpr) {
    FDecl = dyn_cast<FunctionDecl>(DRExpr->getDecl());
    Ovl = dyn_cast<OverloadedFunctionDecl>(DRExpr->getDecl());
  }

  if (Ovl || (getLangOptions().CPlusPlus && (FDecl || UnqualifiedName))) {
    // We don't perform ADL for implicit declarations of builtins.
    if (FDecl && FDecl->getBuiltinID(Context) && FDecl->isImplicit())
      ADL = false;

    // We don't perform ADL in C.
    if (!getLangOptions().CPlusPlus)
      ADL = false;

    if (Ovl || ADL) {
      FDecl = ResolveOverloadedCallFn(Fn, DRExpr? DRExpr->getDecl() : 0,
                                      UnqualifiedName, LParenLoc, Args,
                                      NumArgs, CommaLocs, RParenLoc, ADL);
      if (!FDecl)
        return ExprError();

      // Update Fn to refer to the actual function selected.
      Expr *NewFn = 0;
      if (QualifiedDeclRefExpr *QDRExpr
            = dyn_cast_or_null<QualifiedDeclRefExpr>(DRExpr))
        NewFn = new (Context) QualifiedDeclRefExpr(FDecl, FDecl->getType(),
                                                   QDRExpr->getLocation(),
                                                   false, false,
                                          QDRExpr->getSourceRange().getBegin());
      else
        NewFn = new (Context) DeclRefExpr(FDecl, FDecl->getType(),
                                          Fn->getSourceRange().getBegin());
      Fn->Destroy(Context);
      Fn = NewFn;
    }
  }

  // Promote the function operand.
  UsualUnaryConversions(Fn);

  // Make the call expr early, before semantic checks.  This guarantees cleanup
  // of arguments and function on error.
  ExprOwningPtr<CallExpr> TheCall(this, new (Context) CallExpr(Context, Fn,
                                                               Args, NumArgs,
                                                               Context.BoolTy,
                                                               RParenLoc));

  const FunctionType *FuncT;
  if (!Fn->getType()->isBlockPointerType()) {
    // C99 6.5.2.2p1 - "The expression that denotes the called function shall
    // have type pointer to function".
    const PointerType *PT = Fn->getType()->getAsPointerType();
    if (PT == 0)
      return ExprError(Diag(LParenLoc, diag::err_typecheck_call_not_function)
        << Fn->getType() << Fn->getSourceRange());
    FuncT = PT->getPointeeType()->getAsFunctionType();
  } else { // This is a block call.
    FuncT = Fn->getType()->getAsBlockPointerType()->getPointeeType()->
                getAsFunctionType();
  }
  if (FuncT == 0)
    return ExprError(Diag(LParenLoc, diag::err_typecheck_call_not_function)
      << Fn->getType() << Fn->getSourceRange());

  // We know the result type of the call, set it.
  TheCall->setType(FuncT->getResultType().getNonReferenceType());

  if (const FunctionProtoType *Proto = dyn_cast<FunctionProtoType>(FuncT)) {
    if (ConvertArgumentsForCall(&*TheCall, Fn, FDecl, Proto, Args, NumArgs,
                                RParenLoc))
      return ExprError();
  } else {
    assert(isa<FunctionNoProtoType>(FuncT) && "Unknown FunctionType!");

    // Promote the arguments (C99 6.5.2.2p6).
    for (unsigned i = 0; i != NumArgs; i++) {
      Expr *Arg = Args[i];
      DefaultArgumentPromotion(Arg);
      TheCall->setArg(i, Arg);
    }
  }

  if (CXXMethodDecl *Method = dyn_cast_or_null<CXXMethodDecl>(FDecl))
    if (!Method->isStatic())
      return ExprError(Diag(LParenLoc, diag::err_member_call_without_object)
        << Fn->getSourceRange());

  // Do special checking on direct calls to functions.
  if (FDecl)
    return CheckFunctionCall(FDecl, TheCall.take());

  return Owned(TheCall.take());
}

Action::OwningExprResult
Sema::ActOnCompoundLiteral(SourceLocation LParenLoc, TypeTy *Ty,
                           SourceLocation RParenLoc, ExprArg InitExpr) {
  assert((Ty != 0) && "ActOnCompoundLiteral(): missing type");
  QualType literalType = QualType::getFromOpaquePtr(Ty);
  // FIXME: put back this assert when initializers are worked out.
  //assert((InitExpr != 0) && "ActOnCompoundLiteral(): missing expression");
  Expr *literalExpr = static_cast<Expr*>(InitExpr.get());

  if (literalType->isArrayType()) {
    if (literalType->isVariableArrayType())
      return ExprError(Diag(LParenLoc, diag::err_variable_object_no_init)
        << SourceRange(LParenLoc, literalExpr->getSourceRange().getEnd()));
  } else if (RequireCompleteType(LParenLoc, literalType,
                                    diag::err_typecheck_decl_incomplete_type,
                SourceRange(LParenLoc, literalExpr->getSourceRange().getEnd())))
    return ExprError();

  if (CheckInitializerTypes(literalExpr, literalType, LParenLoc,
                            DeclarationName(), /*FIXME:DirectInit=*/false))
    return ExprError();

  bool isFileScope = getCurFunctionOrMethodDecl() == 0;
  if (isFileScope) { // 6.5.2.5p3
    if (CheckForConstantInitializer(literalExpr, literalType))
      return ExprError();
  }
  InitExpr.release();
  return Owned(new (Context) CompoundLiteralExpr(LParenLoc, literalType,
                                                 literalExpr, isFileScope));
}

Action::OwningExprResult
Sema::ActOnInitList(SourceLocation LBraceLoc, MultiExprArg initlist,
                    InitListDesignations &Designators,
                    SourceLocation RBraceLoc) {
  unsigned NumInit = initlist.size();
  Expr **InitList = reinterpret_cast<Expr**>(initlist.release());

  // Semantic analysis for initializers is done by ActOnDeclarator() and
  // CheckInitializer() - it requires knowledge of the object being intialized.

  InitListExpr *E = new (Context) InitListExpr(LBraceLoc, InitList, NumInit,
                                               RBraceLoc);
  E->setType(Context.VoidTy); // FIXME: just a place holder for now.
  return Owned(E);
}

/// CheckCastTypes - Check type constraints for casting between types.
bool Sema::CheckCastTypes(SourceRange TyR, QualType castType, Expr *&castExpr) {
  UsualUnaryConversions(castExpr);

  // C99 6.5.4p2: the cast type needs to be void or scalar and the expression
  // type needs to be scalar.
  if (castType->isVoidType()) {
    // Cast to void allows any expr type.
  } else if (castType->isDependentType() || castExpr->isTypeDependent()) {
    // We can't check any more until template instantiation time.
  } else if (!castType->isScalarType() && !castType->isVectorType()) {
    if (Context.getCanonicalType(castType).getUnqualifiedType() ==
        Context.getCanonicalType(castExpr->getType().getUnqualifiedType()) &&
        (castType->isStructureType() || castType->isUnionType())) {
      // GCC struct/union extension: allow cast to self.
      Diag(TyR.getBegin(), diag::ext_typecheck_cast_nonscalar)
        << castType << castExpr->getSourceRange();
    } else if (castType->isUnionType()) {
      // GCC cast to union extension
      RecordDecl *RD = castType->getAsRecordType()->getDecl();
      RecordDecl::field_iterator Field, FieldEnd;
      for (Field = RD->field_begin(), FieldEnd = RD->field_end();
           Field != FieldEnd; ++Field) {
        if (Context.getCanonicalType(Field->getType()).getUnqualifiedType() ==
            Context.getCanonicalType(castExpr->getType()).getUnqualifiedType()) {
          Diag(TyR.getBegin(), diag::ext_typecheck_cast_to_union)
            << castExpr->getSourceRange();
          break;
        }
      }
      if (Field == FieldEnd)
        return Diag(TyR.getBegin(), diag::err_typecheck_cast_to_union_no_type)
          << castExpr->getType() << castExpr->getSourceRange();
    } else {
      // Reject any other conversions to non-scalar types.
      return Diag(TyR.getBegin(), diag::err_typecheck_cond_expect_scalar)
        << castType << castExpr->getSourceRange();
    }
  } else if (!castExpr->getType()->isScalarType() &&
             !castExpr->getType()->isVectorType()) {
    return Diag(castExpr->getLocStart(),
                diag::err_typecheck_expect_scalar_operand)
      << castExpr->getType() << castExpr->getSourceRange();
  } else if (castExpr->getType()->isVectorType()) {
    if (CheckVectorCast(TyR, castExpr->getType(), castType))
      return true;
  } else if (castType->isVectorType()) {
    if (CheckVectorCast(TyR, castType, castExpr->getType()))
      return true;
  } else if (getLangOptions().ObjC1 && isa<ObjCSuperExpr>(castExpr)) {
    return Diag(castExpr->getLocStart(), diag::err_illegal_super_cast) << TyR;
  }
  return false;
}

bool Sema::CheckVectorCast(SourceRange R, QualType VectorTy, QualType Ty) {
  assert(VectorTy->isVectorType() && "Not a vector type!");

  if (Ty->isVectorType() || Ty->isIntegerType()) {
    if (Context.getTypeSize(VectorTy) != Context.getTypeSize(Ty))
      return Diag(R.getBegin(),
                  Ty->isVectorType() ?
                  diag::err_invalid_conversion_between_vectors :
                  diag::err_invalid_conversion_between_vector_and_integer)
        << VectorTy << Ty << R;
  } else
    return Diag(R.getBegin(),
                diag::err_invalid_conversion_between_vector_and_scalar)
      << VectorTy << Ty << R;

  return false;
}

Action::OwningExprResult
Sema::ActOnCastExpr(SourceLocation LParenLoc, TypeTy *Ty,
                    SourceLocation RParenLoc, ExprArg Op) {
  assert((Ty != 0) && (Op.get() != 0) &&
         "ActOnCastExpr(): missing type or expr");

  Expr *castExpr = static_cast<Expr*>(Op.release());
  QualType castType = QualType::getFromOpaquePtr(Ty);

  if (CheckCastTypes(SourceRange(LParenLoc, RParenLoc), castType, castExpr))
    return ExprError();
  return Owned(new (Context) CStyleCastExpr(castType, castExpr, castType,
                                            LParenLoc, RParenLoc));
}

/// Note that lhs is not null here, even if this is the gnu "x ?: y" extension.
/// In that case, lhs = cond.
/// C99 6.5.15
QualType Sema::CheckConditionalOperands(Expr *&Cond, Expr *&LHS, Expr *&RHS,
                                        SourceLocation QuestionLoc) {
  UsualUnaryConversions(Cond);
  UsualUnaryConversions(LHS);
  UsualUnaryConversions(RHS);
  QualType CondTy = Cond->getType();
  QualType LHSTy = LHS->getType();
  QualType RHSTy = RHS->getType();

  // first, check the condition.
  if (!Cond->isTypeDependent()) {
    if (!CondTy->isScalarType()) { // C99 6.5.15p2
      Diag(Cond->getLocStart(), diag::err_typecheck_cond_expect_scalar)
        << CondTy;
      return QualType();
    }
  }

  // Now check the two expressions.
  if ((LHS && LHS->isTypeDependent()) || (RHS && RHS->isTypeDependent()))
    return Context.DependentTy;

  // If both operands have arithmetic type, do the usual arithmetic conversions
  // to find a common type: C99 6.5.15p3,5.
  if (LHSTy->isArithmeticType() && RHSTy->isArithmeticType()) {
    UsualArithmeticConversions(LHS, RHS);
    return LHS->getType();
  }

  // If both operands are the same structure or union type, the result is that
  // type.
  if (const RecordType *LHSRT = LHSTy->getAsRecordType()) {    // C99 6.5.15p3
    if (const RecordType *RHSRT = RHSTy->getAsRecordType())
      if (LHSRT->getDecl() == RHSRT->getDecl())
        // "If both the operands have structure or union type, the result has
        // that type."  This implies that CV qualifiers are dropped.
        return LHSTy.getUnqualifiedType();
  }

  // C99 6.5.15p5: "If both operands have void type, the result has void type."
  // The following || allows only one side to be void (a GCC-ism).
  if (LHSTy->isVoidType() || RHSTy->isVoidType()) {
    if (!LHSTy->isVoidType())
      Diag(RHS->getLocStart(), diag::ext_typecheck_cond_one_void)
        << RHS->getSourceRange();
    if (!RHSTy->isVoidType())
      Diag(LHS->getLocStart(), diag::ext_typecheck_cond_one_void)
        << LHS->getSourceRange();
    ImpCastExprToType(LHS, Context.VoidTy);
    ImpCastExprToType(RHS, Context.VoidTy);
    return Context.VoidTy;
  }
  // C99 6.5.15p6 - "if one operand is a null pointer constant, the result has
  // the type of the other operand."
  if ((LHSTy->isPointerType() || LHSTy->isBlockPointerType() ||
       Context.isObjCObjectPointerType(LHSTy)) &&
      RHS->isNullPointerConstant(Context)) {
    ImpCastExprToType(RHS, LHSTy); // promote the null to a pointer.
    return LHSTy;
  }
  if ((RHSTy->isPointerType() || RHSTy->isBlockPointerType() ||
       Context.isObjCObjectPointerType(RHSTy)) &&
      LHS->isNullPointerConstant(Context)) {
    ImpCastExprToType(LHS, RHSTy); // promote the null to a pointer.
    return RHSTy;
  }

  // Handle the case where both operands are pointers before we handle null
  // pointer constants in case both operands are null pointer constants.
  if (const PointerType *LHSPT = LHSTy->getAsPointerType()) { // C99 6.5.15p3,6
    if (const PointerType *RHSPT = RHSTy->getAsPointerType()) {
      // get the "pointed to" types
      QualType lhptee = LHSPT->getPointeeType();
      QualType rhptee = RHSPT->getPointeeType();

      // ignore qualifiers on void (C99 6.5.15p3, clause 6)
      if (lhptee->isVoidType() &&
          rhptee->isIncompleteOrObjectType()) {
        // Figure out necessary qualifiers (C99 6.5.15p6)
        QualType destPointee=lhptee.getQualifiedType(rhptee.getCVRQualifiers());
        QualType destType = Context.getPointerType(destPointee);
        ImpCastExprToType(LHS, destType); // add qualifiers if necessary
        ImpCastExprToType(RHS, destType); // promote to void*
        return destType;
      }
      if (rhptee->isVoidType() && lhptee->isIncompleteOrObjectType()) {
        QualType destPointee=rhptee.getQualifiedType(lhptee.getCVRQualifiers());
        QualType destType = Context.getPointerType(destPointee);
        ImpCastExprToType(LHS, destType); // add qualifiers if necessary
        ImpCastExprToType(RHS, destType); // promote to void*
        return destType;
      }

      if (Context.getCanonicalType(LHSTy) == Context.getCanonicalType(RHSTy)) {
        // Two identical pointer types are always compatible.
        return LHSTy;
      }

      QualType compositeType = LHSTy;

      // If either type is an Objective-C object type then check
      // compatibility according to Objective-C.
      if (Context.isObjCObjectPointerType(LHSTy) ||
          Context.isObjCObjectPointerType(RHSTy)) {
        // If both operands are interfaces and either operand can be
        // assigned to the other, use that type as the composite
        // type. This allows
        //   xxx ? (A*) a : (B*) b
        // where B is a subclass of A.
        //
        // Additionally, as for assignment, if either type is 'id'
        // allow silent coercion. Finally, if the types are
        // incompatible then make sure to use 'id' as the composite
        // type so the result is acceptable for sending messages to.

        // FIXME: Consider unifying with 'areComparableObjCPointerTypes'.
        // It could return the composite type.
        const ObjCInterfaceType* LHSIface = lhptee->getAsObjCInterfaceType();
        const ObjCInterfaceType* RHSIface = rhptee->getAsObjCInterfaceType();
        if (LHSIface && RHSIface &&
            Context.canAssignObjCInterfaces(LHSIface, RHSIface)) {
          compositeType = LHSTy;
        } else if (LHSIface && RHSIface &&
                   Context.canAssignObjCInterfaces(RHSIface, LHSIface)) {
          compositeType = RHSTy;
        } else if (Context.isObjCIdStructType(lhptee) ||
                   Context.isObjCIdStructType(rhptee)) {
          compositeType = Context.getObjCIdType();
        } else {
          Diag(QuestionLoc, diag::ext_typecheck_comparison_of_distinct_pointers)
               << LHSTy << RHSTy
               << LHS->getSourceRange() << RHS->getSourceRange();
          QualType incompatTy = Context.getObjCIdType();
          ImpCastExprToType(LHS, incompatTy);
          ImpCastExprToType(RHS, incompatTy);
          return incompatTy;
        }
      } else if (!Context.typesAreCompatible(lhptee.getUnqualifiedType(),
                                             rhptee.getUnqualifiedType())) {
        Diag(QuestionLoc, diag::warn_typecheck_cond_incompatible_pointers)
          << LHSTy << RHSTy << LHS->getSourceRange() << RHS->getSourceRange();
        // In this situation, we assume void* type. No especially good
        // reason, but this is what gcc does, and we do have to pick
        // to get a consistent AST.
        QualType incompatTy = Context.getPointerType(Context.VoidTy);
        ImpCastExprToType(LHS, incompatTy);
        ImpCastExprToType(RHS, incompatTy);
        return incompatTy;
      }
      // The pointer types are compatible.
      // C99 6.5.15p6: If both operands are pointers to compatible types *or* to
      // differently qualified versions of compatible types, the result type is
      // a pointer to an appropriately qualified version of the *composite*
      // type.
      // FIXME: Need to calculate the composite type.
      // FIXME: Need to add qualifiers
      ImpCastExprToType(LHS, compositeType);
      ImpCastExprToType(RHS, compositeType);
      return compositeType;
    }
  }

  // Selection between block pointer types is ok as long as they are the same.
  if (LHSTy->isBlockPointerType() && RHSTy->isBlockPointerType() &&
      Context.getCanonicalType(LHSTy) == Context.getCanonicalType(RHSTy))
    return LHSTy;

  // Need to handle "id<xx>" explicitly. Unlike "id", whose canonical type
  // evaluates to "struct objc_object *" (and is handled above when comparing
  // id with statically typed objects).
  if (LHSTy->isObjCQualifiedIdType() || RHSTy->isObjCQualifiedIdType()) {
    // GCC allows qualified id and any Objective-C type to devolve to
    // id. Currently localizing to here until clear this should be
    // part of ObjCQualifiedIdTypesAreCompatible.
    if (ObjCQualifiedIdTypesAreCompatible(LHSTy, RHSTy, true) ||
        (LHSTy->isObjCQualifiedIdType() &&
         Context.isObjCObjectPointerType(RHSTy)) ||
        (RHSTy->isObjCQualifiedIdType() &&
         Context.isObjCObjectPointerType(LHSTy))) {
      // FIXME: This is not the correct composite type. This only
      // happens to work because id can more or less be used anywhere,
      // however this may change the type of method sends.
      // FIXME: gcc adds some type-checking of the arguments and emits
      // (confusing) incompatible comparison warnings in some
      // cases. Investigate.
      QualType compositeType = Context.getObjCIdType();
      ImpCastExprToType(LHS, compositeType);
      ImpCastExprToType(RHS, compositeType);
      return compositeType;
    }
  }

  // Otherwise, the operands are not compatible.
  Diag(QuestionLoc, diag::err_typecheck_cond_incompatible_operands)
    << LHSTy << RHSTy << LHS->getSourceRange() << RHS->getSourceRange();
  return QualType();
}

/// ActOnConditionalOp - Parse a ?: operation.  Note that 'LHS' may be null
/// in the case of a the GNU conditional expr extension.
Action::OwningExprResult Sema::ActOnConditionalOp(SourceLocation QuestionLoc,
                                                  SourceLocation ColonLoc,
                                                  ExprArg Cond, ExprArg LHS,
                                                  ExprArg RHS) {
  Expr *CondExpr = (Expr *) Cond.get();
  Expr *LHSExpr = (Expr *) LHS.get(), *RHSExpr = (Expr *) RHS.get();

  // If this is the gnu "x ?: y" extension, analyze the types as though the LHS
  // was the condition.
  bool isLHSNull = LHSExpr == 0;
  if (isLHSNull)
    LHSExpr = CondExpr;

  QualType result = CheckConditionalOperands(CondExpr, LHSExpr,
                                             RHSExpr, QuestionLoc);
  if (result.isNull())
    return ExprError();

  Cond.release();
  LHS.release();
  RHS.release();
  return Owned(new (Context) ConditionalOperator(CondExpr,
                                                 isLHSNull ? 0 : LHSExpr,
                                                 RHSExpr, result));
}


// CheckPointerTypesForAssignment - This is a very tricky routine (despite
// being closely modeled after the C99 spec:-). The odd characteristic of this
// routine is it effectively iqnores the qualifiers on the top level pointee.
// This circumvents the usual type rules specified in 6.2.7p1 & 6.7.5.[1-3].
// FIXME: add a couple examples in this comment.
Sema::AssignConvertType
Sema::CheckPointerTypesForAssignment(QualType lhsType, QualType rhsType) {
  QualType lhptee, rhptee;

  // get the "pointed to" type (ignoring qualifiers at the top level)
  lhptee = lhsType->getAsPointerType()->getPointeeType();
  rhptee = rhsType->getAsPointerType()->getPointeeType();

  // make sure we operate on the canonical type
  lhptee = Context.getCanonicalType(lhptee);
  rhptee = Context.getCanonicalType(rhptee);

  AssignConvertType ConvTy = Compatible;

  // C99 6.5.16.1p1: This following citation is common to constraints
  // 3 & 4 (below). ...and the type *pointed to* by the left has all the
  // qualifiers of the type *pointed to* by the right;
  // FIXME: Handle ExtQualType
  if (!lhptee.isAtLeastAsQualifiedAs(rhptee))
    ConvTy = CompatiblePointerDiscardsQualifiers;

  // C99 6.5.16.1p1 (constraint 4): If one operand is a pointer to an object or
  // incomplete type and the other is a pointer to a qualified or unqualified
  // version of void...
  if (lhptee->isVoidType()) {
    if (rhptee->isIncompleteOrObjectType())
      return ConvTy;

    // As an extension, we allow cast to/from void* to function pointer.
    assert(rhptee->isFunctionType());
    return FunctionVoidPointer;
  }

  if (rhptee->isVoidType()) {
    if (lhptee->isIncompleteOrObjectType())
      return ConvTy;

    // As an extension, we allow cast to/from void* to function pointer.
    assert(lhptee->isFunctionType());
    return FunctionVoidPointer;
  }
  // C99 6.5.16.1p1 (constraint 3): both operands are pointers to qualified or
  // unqualified versions of compatible types, ...
  if (!Context.typesAreCompatible(lhptee.getUnqualifiedType(),
                                  rhptee.getUnqualifiedType()))
    return IncompatiblePointer; // this "trumps" PointerAssignDiscardsQualifiers
  return ConvTy;
}

/// CheckBlockPointerTypesForAssignment - This routine determines whether two
/// block pointer types are compatible or whether a block and normal pointer
/// are compatible. It is more restrict than comparing two function pointer
// types.
Sema::AssignConvertType
Sema::CheckBlockPointerTypesForAssignment(QualType lhsType,
                                          QualType rhsType) {
  QualType lhptee, rhptee;

  // get the "pointed to" type (ignoring qualifiers at the top level)
  lhptee = lhsType->getAsBlockPointerType()->getPointeeType();
  rhptee = rhsType->getAsBlockPointerType()->getPointeeType();

  // make sure we operate on the canonical type
  lhptee = Context.getCanonicalType(lhptee);
  rhptee = Context.getCanonicalType(rhptee);

  AssignConvertType ConvTy = Compatible;

  // For blocks we enforce that qualifiers are identical.
  if (lhptee.getCVRQualifiers() != rhptee.getCVRQualifiers())
    ConvTy = CompatiblePointerDiscardsQualifiers;

  if (!Context.typesAreBlockCompatible(lhptee, rhptee))
    return IncompatibleBlockPointer;
  return ConvTy;
}

/// CheckAssignmentConstraints (C99 6.5.16) - This routine currently
/// has code to accommodate several GCC extensions when type checking
/// pointers. Here are some objectionable examples that GCC considers warnings:
///
///  int a, *pint;
///  short *pshort;
///  struct foo *pfoo;
///
///  pint = pshort; // warning: assignment from incompatible pointer type
///  a = pint; // warning: assignment makes integer from pointer without a cast
///  pint = a; // warning: assignment makes pointer from integer without a cast
///  pint = pfoo; // warning: assignment from incompatible pointer type
///
/// As a result, the code for dealing with pointers is more complex than the
/// C99 spec dictates.
///
Sema::AssignConvertType
Sema::CheckAssignmentConstraints(QualType lhsType, QualType rhsType) {
  // Get canonical types.  We're not formatting these types, just comparing
  // them.
  lhsType = Context.getCanonicalType(lhsType).getUnqualifiedType();
  rhsType = Context.getCanonicalType(rhsType).getUnqualifiedType();

  if (lhsType == rhsType)
    return Compatible; // Common case: fast path an exact match.

  // If the left-hand side is a reference type, then we are in a
  // (rare!) case where we've allowed the use of references in C,
  // e.g., as a parameter type in a built-in function. In this case,
  // just make sure that the type referenced is compatible with the
  // right-hand side type. The caller is responsible for adjusting
  // lhsType so that the resulting expression does not have reference
  // type.
  if (const ReferenceType *lhsTypeRef = lhsType->getAsReferenceType()) {
    if (Context.typesAreCompatible(lhsTypeRef->getPointeeType(), rhsType))
      return Compatible;
    return Incompatible;
  }

  if (lhsType->isObjCQualifiedIdType() || rhsType->isObjCQualifiedIdType()) {
    if (ObjCQualifiedIdTypesAreCompatible(lhsType, rhsType, false))
      return Compatible;
    // Relax integer conversions like we do for pointers below.
    if (rhsType->isIntegerType())
      return IntToPointer;
    if (lhsType->isIntegerType())
      return PointerToInt;
    return IncompatibleObjCQualifiedId;
  }

  if (lhsType->isVectorType() || rhsType->isVectorType()) {
    // For ExtVector, allow vector splats; float -> <n x float>
    if (const ExtVectorType *LV = lhsType->getAsExtVectorType())
      if (LV->getElementType() == rhsType)
        return Compatible;

    // If we are allowing lax vector conversions, and LHS and RHS are both
    // vectors, the total size only needs to be the same. This is a bitcast;
    // no bits are changed but the result type is different.
    if (getLangOptions().LaxVectorConversions &&
        lhsType->isVectorType() && rhsType->isVectorType()) {
      if (Context.getTypeSize(lhsType) == Context.getTypeSize(rhsType))
        return IncompatibleVectors;
    }
    return Incompatible;
  }

  if (lhsType->isArithmeticType() && rhsType->isArithmeticType())
    return Compatible;

  if (isa<PointerType>(lhsType)) {
    if (rhsType->isIntegerType())
      return IntToPointer;

    if (isa<PointerType>(rhsType))
      return CheckPointerTypesForAssignment(lhsType, rhsType);

    if (rhsType->getAsBlockPointerType()) {
      if (lhsType->getAsPointerType()->getPointeeType()->isVoidType())
        return Compatible;

      // Treat block pointers as objects.
      if (getLangOptions().ObjC1 &&
          lhsType == Context.getCanonicalType(Context.getObjCIdType()))
        return Compatible;
    }
    return Incompatible;
  }

  if (isa<BlockPointerType>(lhsType)) {
    if (rhsType->isIntegerType())
      return IntToBlockPointer;

    // Treat block pointers as objects.
    if (getLangOptions().ObjC1 &&
        rhsType == Context.getCanonicalType(Context.getObjCIdType()))
      return Compatible;

    if (rhsType->isBlockPointerType())
      return CheckBlockPointerTypesForAssignment(lhsType, rhsType);

    if (const PointerType *RHSPT = rhsType->getAsPointerType()) {
      if (RHSPT->getPointeeType()->isVoidType())
        return Compatible;
    }
    return Incompatible;
  }

  if (isa<PointerType>(rhsType)) {
    // C99 6.5.16.1p1: the left operand is _Bool and the right is a pointer.
    if (lhsType == Context.BoolTy)
      return Compatible;

    if (lhsType->isIntegerType())
      return PointerToInt;

    if (isa<PointerType>(lhsType))
      return CheckPointerTypesForAssignment(lhsType, rhsType);

    if (isa<BlockPointerType>(lhsType) &&
        rhsType->getAsPointerType()->getPointeeType()->isVoidType())
      return Compatible;
    return Incompatible;
  }

  if (isa<TagType>(lhsType) && isa<TagType>(rhsType)) {
    if (Context.typesAreCompatible(lhsType, rhsType))
      return Compatible;
  }
  return Incompatible;
}

Sema::AssignConvertType
Sema::CheckSingleAssignmentConstraints(QualType lhsType, Expr *&rExpr) {
  if (getLangOptions().CPlusPlus) {
    if (!lhsType->isRecordType()) {
      // C++ 5.17p3: If the left operand is not of class type, the
      // expression is implicitly converted (C++ 4) to the
      // cv-unqualified type of the left operand.
      if (PerformImplicitConversion(rExpr, lhsType.getUnqualifiedType(),
                                    "assigning"))
        return Incompatible;
      else
        return Compatible;
    }

    // FIXME: Currently, we fall through and treat C++ classes like C
    // structures.
  }

  // C99 6.5.16.1p1: the left operand is a pointer and the right is
  // a null pointer constant.
  if ((lhsType->isPointerType() || 
       lhsType->isObjCQualifiedIdType() || 
       lhsType->isBlockPointerType())
      && rExpr->isNullPointerConstant(Context)) {
    ImpCastExprToType(rExpr, lhsType);
    return Compatible;
  }

  // This check seems unnatural, however it is necessary to ensure the proper
  // conversion of functions/arrays. If the conversion were done for all
  // DeclExpr's (created by ActOnIdentifierExpr), it would mess up the unary
  // expressions that surpress this implicit conversion (&, sizeof).
  //
  // Suppress this for references: C++ 8.5.3p5.
  if (!lhsType->isReferenceType())
    DefaultFunctionArrayConversion(rExpr);

  Sema::AssignConvertType result =
    CheckAssignmentConstraints(lhsType, rExpr->getType());

  // C99 6.5.16.1p2: The value of the right operand is converted to the
  // type of the assignment expression.
  // CheckAssignmentConstraints allows the left-hand side to be a reference,
  // so that we can use references in built-in functions even in C.
  // The getNonReferenceType() call makes sure that the resulting expression
  // does not have reference type.
  if (rExpr->getType() != lhsType)
    ImpCastExprToType(rExpr, lhsType.getNonReferenceType());
  return result;
}

Sema::AssignConvertType
Sema::CheckCompoundAssignmentConstraints(QualType lhsType, QualType rhsType) {
  return CheckAssignmentConstraints(lhsType, rhsType);
}

QualType Sema::InvalidOperands(SourceLocation Loc, Expr *&lex, Expr *&rex) {
  Diag(Loc, diag::err_typecheck_invalid_operands)
    << lex->getType() << rex->getType()
    << lex->getSourceRange() << rex->getSourceRange();
  return QualType();
}

inline QualType Sema::CheckVectorOperands(SourceLocation Loc, Expr *&lex,
                                                              Expr *&rex) {
  // For conversion purposes, we ignore any qualifiers.
  // For example, "const float" and "float" are equivalent.
  QualType lhsType =
    Context.getCanonicalType(lex->getType()).getUnqualifiedType();
  QualType rhsType =
    Context.getCanonicalType(rex->getType()).getUnqualifiedType();

  // If the vector types are identical, return.
  if (lhsType == rhsType)
    return lhsType;

  // Handle the case of a vector & extvector type of the same size and element
  // type.  It would be nice if we only had one vector type someday.
  if (getLangOptions().LaxVectorConversions) {
    // FIXME: Should we warn here?
    if (const VectorType *LV = lhsType->getAsVectorType()) {
      if (const VectorType *RV = rhsType->getAsVectorType())
        if (LV->getElementType() == RV->getElementType() &&
            LV->getNumElements() == RV->getNumElements()) {
          return lhsType->isExtVectorType() ? lhsType : rhsType;
        }
    }
  }

  // If the lhs is an extended vector and the rhs is a scalar of the same type
  // or a literal, promote the rhs to the vector type.
  if (const ExtVectorType *V = lhsType->getAsExtVectorType()) {
    QualType eltType = V->getElementType();

    if ((eltType->getAsBuiltinType() == rhsType->getAsBuiltinType()) ||
        (eltType->isIntegerType() && isa<IntegerLiteral>(rex)) ||
        (eltType->isFloatingType() && isa<FloatingLiteral>(rex))) {
      ImpCastExprToType(rex, lhsType);
      return lhsType;
    }
  }

  // If the rhs is an extended vector and the lhs is a scalar of the same type,
  // promote the lhs to the vector type.
  if (const ExtVectorType *V = rhsType->getAsExtVectorType()) {
    QualType eltType = V->getElementType();

    if ((eltType->getAsBuiltinType() == lhsType->getAsBuiltinType()) ||
        (eltType->isIntegerType() && isa<IntegerLiteral>(lex)) ||
        (eltType->isFloatingType() && isa<FloatingLiteral>(lex))) {
      ImpCastExprToType(lex, rhsType);
      return rhsType;
    }
  }

  // You cannot convert between vector values of different size.
  Diag(Loc, diag::err_typecheck_vector_not_convertable)
    << lex->getType() << rex->getType()
    << lex->getSourceRange() << rex->getSourceRange();
  return QualType();
}

inline QualType Sema::CheckMultiplyDivideOperands(
  Expr *&lex, Expr *&rex, SourceLocation Loc, bool isCompAssign)
{
  if (lex->getType()->isVectorType() || rex->getType()->isVectorType())
    return CheckVectorOperands(Loc, lex, rex);

  QualType compType = UsualArithmeticConversions(lex, rex, isCompAssign);

  if (lex->getType()->isArithmeticType() && rex->getType()->isArithmeticType())
    return compType;
  return InvalidOperands(Loc, lex, rex);
}

inline QualType Sema::CheckRemainderOperands(
  Expr *&lex, Expr *&rex, SourceLocation Loc, bool isCompAssign)
{
  if (lex->getType()->isVectorType() || rex->getType()->isVectorType()) {
    if (lex->getType()->isIntegerType() && rex->getType()->isIntegerType())
      return CheckVectorOperands(Loc, lex, rex);
    return InvalidOperands(Loc, lex, rex);
  }

  QualType compType = UsualArithmeticConversions(lex, rex, isCompAssign);

  if (lex->getType()->isIntegerType() && rex->getType()->isIntegerType())
    return compType;
  return InvalidOperands(Loc, lex, rex);
}

inline QualType Sema::CheckAdditionOperands( // C99 6.5.6
  Expr *&lex, Expr *&rex, SourceLocation Loc, bool isCompAssign)
{
  if (lex->getType()->isVectorType() || rex->getType()->isVectorType())
    return CheckVectorOperands(Loc, lex, rex);

  QualType compType = UsualArithmeticConversions(lex, rex, isCompAssign);

  // handle the common case first (both operands are arithmetic).
  if (lex->getType()->isArithmeticType() && rex->getType()->isArithmeticType())
    return compType;

  // Put any potential pointer into PExp
  Expr* PExp = lex, *IExp = rex;
  if (IExp->getType()->isPointerType())
    std::swap(PExp, IExp);

  if (const PointerType* PTy = PExp->getType()->getAsPointerType()) {
    if (IExp->getType()->isIntegerType()) {
      // Check for arithmetic on pointers to incomplete types
      if (!PTy->getPointeeType()->isObjectType()) {
        if (PTy->getPointeeType()->isVoidType()) {
          if (getLangOptions().CPlusPlus) {
            Diag(Loc, diag::err_typecheck_pointer_arith_void_type)
              << lex->getSourceRange() << rex->getSourceRange();
            return QualType();
          }

          // GNU extension: arithmetic on pointer to void
          Diag(Loc, diag::ext_gnu_void_ptr)
            << lex->getSourceRange() << rex->getSourceRange();
        } else if (PTy->getPointeeType()->isFunctionType()) {
          if (getLangOptions().CPlusPlus) {
            Diag(Loc, diag::err_typecheck_pointer_arith_function_type)
              << lex->getType() << lex->getSourceRange();
            return QualType();
          }

          // GNU extension: arithmetic on pointer to function
          Diag(Loc, diag::ext_gnu_ptr_func_arith)
            << lex->getType() << lex->getSourceRange();
        } else {
          RequireCompleteType(Loc, PTy->getPointeeType(),
                                 diag::err_typecheck_arithmetic_incomplete_type,
                                 lex->getSourceRange(), SourceRange(),
                                 lex->getType());
          return QualType();
        }
      }
      return PExp->getType();
    }
  }

  return InvalidOperands(Loc, lex, rex);
}

// C99 6.5.6
QualType Sema::CheckSubtractionOperands(Expr *&lex, Expr *&rex,
                                        SourceLocation Loc, bool isCompAssign) {
  if (lex->getType()->isVectorType() || rex->getType()->isVectorType())
    return CheckVectorOperands(Loc, lex, rex);

  QualType compType = UsualArithmeticConversions(lex, rex, isCompAssign);

  // Enforce type constraints: C99 6.5.6p3.

  // Handle the common case first (both operands are arithmetic).
  if (lex->getType()->isArithmeticType() && rex->getType()->isArithmeticType())
    return compType;

  // Either ptr - int   or   ptr - ptr.
  if (const PointerType *LHSPTy = lex->getType()->getAsPointerType()) {
    QualType lpointee = LHSPTy->getPointeeType();

    // The LHS must be an object type, not incomplete, function, etc.
    if (!lpointee->isObjectType()) {
      // Handle the GNU void* extension.
      if (lpointee->isVoidType()) {
        Diag(Loc, diag::ext_gnu_void_ptr)
          << lex->getSourceRange() << rex->getSourceRange();
      } else if (lpointee->isFunctionType()) {
        if (getLangOptions().CPlusPlus) {
          Diag(Loc, diag::err_typecheck_pointer_arith_function_type)
            << lex->getType() << lex->getSourceRange();
          return QualType();
        }

        // GNU extension: arithmetic on pointer to function
        Diag(Loc, diag::ext_gnu_ptr_func_arith)
          << lex->getType() << lex->getSourceRange();
      } else {
        Diag(Loc, diag::err_typecheck_sub_ptr_object)
          << lex->getType() << lex->getSourceRange();
        return QualType();
      }
    }

    // The result type of a pointer-int computation is the pointer type.
    if (rex->getType()->isIntegerType())
      return lex->getType();

    // Handle pointer-pointer subtractions.
    if (const PointerType *RHSPTy = rex->getType()->getAsPointerType()) {
      QualType rpointee = RHSPTy->getPointeeType();

      // RHS must be an object type, unless void (GNU).
      if (!rpointee->isObjectType()) {
        // Handle the GNU void* extension.
        if (rpointee->isVoidType()) {
          if (!lpointee->isVoidType())
            Diag(Loc, diag::ext_gnu_void_ptr)
              << lex->getSourceRange() << rex->getSourceRange();
        } else if (rpointee->isFunctionType()) {
          if (getLangOptions().CPlusPlus) {
            Diag(Loc, diag::err_typecheck_pointer_arith_function_type)
              << rex->getType() << rex->getSourceRange();
            return QualType();
          }

          // GNU extension: arithmetic on pointer to function
          if (!lpointee->isFunctionType())
            Diag(Loc, diag::ext_gnu_ptr_func_arith)
              << lex->getType() << lex->getSourceRange();
        } else {
          Diag(Loc, diag::err_typecheck_sub_ptr_object)
            << rex->getType() << rex->getSourceRange();
          return QualType();
        }
      }

      // Pointee types must be compatible.
      if (!Context.typesAreCompatible(
              Context.getCanonicalType(lpointee).getUnqualifiedType(),
              Context.getCanonicalType(rpointee).getUnqualifiedType())) {
        Diag(Loc, diag::err_typecheck_sub_ptr_compatible)
          << lex->getType() << rex->getType()
          << lex->getSourceRange() << rex->getSourceRange();
        return QualType();
      }

      return Context.getPointerDiffType();
    }
  }

  return InvalidOperands(Loc, lex, rex);
}

// C99 6.5.7
QualType Sema::CheckShiftOperands(Expr *&lex, Expr *&rex, SourceLocation Loc,
                                  bool isCompAssign) {
  // C99 6.5.7p2: Each of the operands shall have integer type.
  if (!lex->getType()->isIntegerType() || !rex->getType()->isIntegerType())
    return InvalidOperands(Loc, lex, rex);

  // Shifts don't perform usual arithmetic conversions, they just do integer
  // promotions on each operand. C99 6.5.7p3
  if (!isCompAssign)
    UsualUnaryConversions(lex);
  UsualUnaryConversions(rex);

  // "The type of the result is that of the promoted left operand."
  return lex->getType();
}

// C99 6.5.8
QualType Sema::CheckCompareOperands(Expr *&lex, Expr *&rex, SourceLocation Loc,
                                    bool isRelational) {
  if (lex->getType()->isVectorType() || rex->getType()->isVectorType())
    return CheckVectorCompareOperands(lex, rex, Loc, isRelational);

  // C99 6.5.8p3 / C99 6.5.9p4
  if (lex->getType()->isArithmeticType() && rex->getType()->isArithmeticType())
    UsualArithmeticConversions(lex, rex);
  else {
    UsualUnaryConversions(lex);
    UsualUnaryConversions(rex);
  }
  QualType lType = lex->getType();
  QualType rType = rex->getType();

  if (!lType->isFloatingType()) {
    // For non-floating point types, check for self-comparisons of the form
    // x == x, x != x, x < x, etc.  These always evaluate to a constant, and
    // often indicate logic errors in the program.
    Expr *LHSStripped = lex->IgnoreParens();
    Expr *RHSStripped = rex->IgnoreParens();
    if (DeclRefExpr* DRL = dyn_cast<DeclRefExpr>(LHSStripped))
      if (DeclRefExpr* DRR = dyn_cast<DeclRefExpr>(RHSStripped))
        if (DRL->getDecl() == DRR->getDecl())
          Diag(Loc, diag::warn_selfcomparison);
    
    if (isa<CastExpr>(LHSStripped))
      LHSStripped = LHSStripped->IgnoreParenCasts();
    if (isa<CastExpr>(RHSStripped))
      RHSStripped = RHSStripped->IgnoreParenCasts();
    
    // Warn about comparisons against a string constant (unless the other
    // operand is null), the user probably wants strcmp.
    if ((isa<StringLiteral>(LHSStripped) || isa<ObjCEncodeExpr>(LHSStripped)) &&
        !RHSStripped->isNullPointerConstant(Context))
      Diag(Loc, diag::warn_stringcompare) << lex->getSourceRange();
    else if ((isa<StringLiteral>(RHSStripped) ||
              isa<ObjCEncodeExpr>(RHSStripped)) &&
             !LHSStripped->isNullPointerConstant(Context))
      Diag(Loc, diag::warn_stringcompare) << rex->getSourceRange();
  }

  // The result of comparisons is 'bool' in C++, 'int' in C.
  QualType ResultTy = getLangOptions().CPlusPlus? Context.BoolTy :Context.IntTy;

  if (isRelational) {
    if (lType->isRealType() && rType->isRealType())
      return ResultTy;
  } else {
    // Check for comparisons of floating point operands using != and ==.
    if (lType->isFloatingType()) {
      assert(rType->isFloatingType());
      CheckFloatComparison(Loc,lex,rex);
    }

    if (lType->isArithmeticType() && rType->isArithmeticType())
      return ResultTy;
  }

  bool LHSIsNull = lex->isNullPointerConstant(Context);
  bool RHSIsNull = rex->isNullPointerConstant(Context);

  // All of the following pointer related warnings are GCC extensions, except
  // when handling null pointer constants. One day, we can consider making them
  // errors (when -pedantic-errors is enabled).
  if (lType->isPointerType() && rType->isPointerType()) { // C99 6.5.8p2
    QualType LCanPointeeTy =
      Context.getCanonicalType(lType->getAsPointerType()->getPointeeType());
    QualType RCanPointeeTy =
      Context.getCanonicalType(rType->getAsPointerType()->getPointeeType());

    if (!LHSIsNull && !RHSIsNull &&                       // C99 6.5.9p2
        !LCanPointeeTy->isVoidType() && !RCanPointeeTy->isVoidType() &&
        !Context.typesAreCompatible(LCanPointeeTy.getUnqualifiedType(),
                                    RCanPointeeTy.getUnqualifiedType()) &&
        !Context.areComparableObjCPointerTypes(lType, rType)) {
      Diag(Loc, diag::ext_typecheck_comparison_of_distinct_pointers)
        << lType << rType << lex->getSourceRange() << rex->getSourceRange();
    }
    ImpCastExprToType(rex, lType); // promote the pointer to pointer
    return ResultTy;
  }
  // Handle block pointer types.
  if (lType->isBlockPointerType() && rType->isBlockPointerType()) {
    QualType lpointee = lType->getAsBlockPointerType()->getPointeeType();
    QualType rpointee = rType->getAsBlockPointerType()->getPointeeType();

    if (!LHSIsNull && !RHSIsNull &&
        !Context.typesAreBlockCompatible(lpointee, rpointee)) {
      Diag(Loc, diag::err_typecheck_comparison_of_distinct_blocks)
        << lType << rType << lex->getSourceRange() << rex->getSourceRange();
    }
    ImpCastExprToType(rex, lType); // promote the pointer to pointer
    return ResultTy;
  }
  // Allow block pointers to be compared with null pointer constants.
  if ((lType->isBlockPointerType() && rType->isPointerType()) ||
      (lType->isPointerType() && rType->isBlockPointerType())) {
    if (!LHSIsNull && !RHSIsNull) {
      Diag(Loc, diag::err_typecheck_comparison_of_distinct_blocks)
        << lType << rType << lex->getSourceRange() << rex->getSourceRange();
    }
    ImpCastExprToType(rex, lType); // promote the pointer to pointer
    return ResultTy;
  }

  if ((lType->isObjCQualifiedIdType() || rType->isObjCQualifiedIdType())) {
    if (lType->isPointerType() || rType->isPointerType()) {
      const PointerType *LPT = lType->getAsPointerType();
      const PointerType *RPT = rType->getAsPointerType();
      bool LPtrToVoid = LPT ?
        Context.getCanonicalType(LPT->getPointeeType())->isVoidType() : false;
      bool RPtrToVoid = RPT ?
        Context.getCanonicalType(RPT->getPointeeType())->isVoidType() : false;

      if (!LPtrToVoid && !RPtrToVoid &&
          !Context.typesAreCompatible(lType, rType)) {
        Diag(Loc, diag::ext_typecheck_comparison_of_distinct_pointers)
          << lType << rType << lex->getSourceRange() << rex->getSourceRange();
        ImpCastExprToType(rex, lType);
        return ResultTy;
      }
      ImpCastExprToType(rex, lType);
      return ResultTy;
    }
    if (ObjCQualifiedIdTypesAreCompatible(lType, rType, true)) {
      ImpCastExprToType(rex, lType);
      return ResultTy;
    } else {
      if ((lType->isObjCQualifiedIdType() && rType->isObjCQualifiedIdType())) {
        Diag(Loc, diag::warn_incompatible_qualified_id_operands)
          << lType << rType << lex->getSourceRange() << rex->getSourceRange();
        ImpCastExprToType(rex, lType);
        return ResultTy;
      }
    }
  }
  if ((lType->isPointerType() || lType->isObjCQualifiedIdType()) &&
       rType->isIntegerType()) {
    if (!RHSIsNull)
      Diag(Loc, diag::ext_typecheck_comparison_of_pointer_integer)
        << lType << rType << lex->getSourceRange() << rex->getSourceRange();
    ImpCastExprToType(rex, lType); // promote the integer to pointer
    return ResultTy;
  }
  if (lType->isIntegerType() &&
      (rType->isPointerType() || rType->isObjCQualifiedIdType())) {
    if (!LHSIsNull)
      Diag(Loc, diag::ext_typecheck_comparison_of_pointer_integer)
        << lType << rType << lex->getSourceRange() << rex->getSourceRange();
    ImpCastExprToType(lex, rType); // promote the integer to pointer
    return ResultTy;
  }
  // Handle block pointers.
  if (lType->isBlockPointerType() && rType->isIntegerType()) {
    if (!RHSIsNull)
      Diag(Loc, diag::ext_typecheck_comparison_of_pointer_integer)
        << lType << rType << lex->getSourceRange() << rex->getSourceRange();
    ImpCastExprToType(rex, lType); // promote the integer to pointer
    return ResultTy;
  }
  if (lType->isIntegerType() && rType->isBlockPointerType()) {
    if (!LHSIsNull)
      Diag(Loc, diag::ext_typecheck_comparison_of_pointer_integer)
        << lType << rType << lex->getSourceRange() << rex->getSourceRange();
    ImpCastExprToType(lex, rType); // promote the integer to pointer
    return ResultTy;
  }
  return InvalidOperands(Loc, lex, rex);
}

/// CheckVectorCompareOperands - vector comparisons are a clang extension that
/// operates on extended vector types.  Instead of producing an IntTy result,
/// like a scalar comparison, a vector comparison produces a vector of integer
/// types.
QualType Sema::CheckVectorCompareOperands(Expr *&lex, Expr *&rex,
                                          SourceLocation Loc,
                                          bool isRelational) {
  // Check to make sure we're operating on vectors of the same type and width,
  // Allowing one side to be a scalar of element type.
  QualType vType = CheckVectorOperands(Loc, lex, rex);
  if (vType.isNull())
    return vType;

  QualType lType = lex->getType();
  QualType rType = rex->getType();

  // For non-floating point types, check for self-comparisons of the form
  // x == x, x != x, x < x, etc.  These always evaluate to a constant, and
  // often indicate logic errors in the program.
  if (!lType->isFloatingType()) {
    if (DeclRefExpr* DRL = dyn_cast<DeclRefExpr>(lex->IgnoreParens()))
      if (DeclRefExpr* DRR = dyn_cast<DeclRefExpr>(rex->IgnoreParens()))
        if (DRL->getDecl() == DRR->getDecl())
          Diag(Loc, diag::warn_selfcomparison);
  }

  // Check for comparisons of floating point operands using != and ==.
  if (!isRelational && lType->isFloatingType()) {
    assert (rType->isFloatingType());
    CheckFloatComparison(Loc,lex,rex);
  }

  // Return the type for the comparison, which is the same as vector type for
  // integer vectors, or an integer type of identical size and number of
  // elements for floating point vectors.
  if (lType->isIntegerType())
    return lType;

  const VectorType *VTy = lType->getAsVectorType();
  unsigned TypeSize = Context.getTypeSize(VTy->getElementType());
  if (TypeSize == Context.getTypeSize(Context.IntTy))
    return Context.getExtVectorType(Context.IntTy, VTy->getNumElements());
  else if (TypeSize == Context.getTypeSize(Context.LongTy))
    return Context.getExtVectorType(Context.LongTy, VTy->getNumElements());

  assert(TypeSize == Context.getTypeSize(Context.LongLongTy) &&
         "Unhandled vector element size in vector compare");
  return Context.getExtVectorType(Context.LongLongTy, VTy->getNumElements());
}

inline QualType Sema::CheckBitwiseOperands(
  Expr *&lex, Expr *&rex, SourceLocation Loc, bool isCompAssign)
{
  if (lex->getType()->isVectorType() || rex->getType()->isVectorType())
    return CheckVectorOperands(Loc, lex, rex);

  QualType compType = UsualArithmeticConversions(lex, rex, isCompAssign);

  if (lex->getType()->isIntegerType() && rex->getType()->isIntegerType())
    return compType;
  return InvalidOperands(Loc, lex, rex);
}

inline QualType Sema::CheckLogicalOperands( // C99 6.5.[13,14]
  Expr *&lex, Expr *&rex, SourceLocation Loc)
{
  UsualUnaryConversions(lex);
  UsualUnaryConversions(rex);

  if (lex->getType()->isScalarType() && rex->getType()->isScalarType())
    return Context.IntTy;
  return InvalidOperands(Loc, lex, rex);
}

/// IsReadonlyProperty - Verify that otherwise a valid l-value expression
/// is a read-only property; return true if so. A readonly property expression
/// depends on various declarations and thus must be treated specially.
///
static bool IsReadonlyProperty(Expr *E, Sema &S)
{
  if (E->getStmtClass() == Expr::ObjCPropertyRefExprClass) {
    const ObjCPropertyRefExpr* PropExpr = cast<ObjCPropertyRefExpr>(E);
    if (ObjCPropertyDecl *PDecl = PropExpr->getProperty()) {
      QualType BaseType = PropExpr->getBase()->getType();
      if (const PointerType *PTy = BaseType->getAsPointerType())
        if (const ObjCInterfaceType *IFTy =
            PTy->getPointeeType()->getAsObjCInterfaceType())
          if (ObjCInterfaceDecl *IFace = IFTy->getDecl())
            if (S.isPropertyReadonly(PDecl, IFace))
              return true;
    }
  }
  return false;
}

/// CheckForModifiableLvalue - Verify that E is a modifiable lvalue.  If not,
/// emit an error and return true.  If so, return false.
static bool CheckForModifiableLvalue(Expr *E, SourceLocation Loc, Sema &S) {
  Expr::isModifiableLvalueResult IsLV = E->isModifiableLvalue(S.Context);
  if (IsLV == Expr::MLV_Valid && IsReadonlyProperty(E, S))
    IsLV = Expr::MLV_ReadonlyProperty;
  if (IsLV == Expr::MLV_Valid)
    return false;

  unsigned Diag = 0;
  bool NeedType = false;
  switch (IsLV) { // C99 6.5.16p2
  default: assert(0 && "Unknown result from isModifiableLvalue!");
  case Expr::MLV_ConstQualified: Diag = diag::err_typecheck_assign_const; break;
  case Expr::MLV_ArrayType:
    Diag = diag::err_typecheck_array_not_modifiable_lvalue;
    NeedType = true;
    break;
  case Expr::MLV_NotObjectType:
    Diag = diag::err_typecheck_non_object_not_modifiable_lvalue;
    NeedType = true;
    break;
  case Expr::MLV_LValueCast:
    Diag = diag::err_typecheck_lvalue_casts_not_supported;
    break;
  case Expr::MLV_InvalidExpression:
    Diag = diag::err_typecheck_expression_not_modifiable_lvalue;
    break;
  case Expr::MLV_IncompleteType:
  case Expr::MLV_IncompleteVoidType:
    return S.RequireCompleteType(Loc, E->getType(),
                      diag::err_typecheck_incomplete_type_not_modifiable_lvalue,
                                    E->getSourceRange());
  case Expr::MLV_DuplicateVectorComponents:
    Diag = diag::err_typecheck_duplicate_vector_components_not_mlvalue;
    break;
  case Expr::MLV_NotBlockQualified:
    Diag = diag::err_block_decl_ref_not_modifiable_lvalue;
    break;
  case Expr::MLV_ReadonlyProperty:
    Diag = diag::error_readonly_property_assignment;
    break;
  case Expr::MLV_NoSetterProperty:
    Diag = diag::error_nosetter_property_assignment;
    break;
  }

  if (NeedType)
    S.Diag(Loc, Diag) << E->getType() << E->getSourceRange();
  else
    S.Diag(Loc, Diag) << E->getSourceRange();
  return true;
}



// C99 6.5.16.1
QualType Sema::CheckAssignmentOperands(Expr *LHS, Expr *&RHS,
                                       SourceLocation Loc,
                                       QualType CompoundType) {
  // Verify that LHS is a modifiable lvalue, and emit error if not.
  if (CheckForModifiableLvalue(LHS, Loc, *this))
    return QualType();

  QualType LHSType = LHS->getType();
  QualType RHSType = CompoundType.isNull() ? RHS->getType() : CompoundType;

  AssignConvertType ConvTy;
  if (CompoundType.isNull()) {
    // Simple assignment "x = y".
    ConvTy = CheckSingleAssignmentConstraints(LHSType, RHS);
    // Special case of NSObject attributes on c-style pointer types.
    if (ConvTy == IncompatiblePointer &&
        ((Context.isObjCNSObjectType(LHSType) &&
          Context.isObjCObjectPointerType(RHSType)) ||
         (Context.isObjCNSObjectType(RHSType) &&
          Context.isObjCObjectPointerType(LHSType))))
      ConvTy = Compatible;

    // If the RHS is a unary plus or minus, check to see if they = and + are
    // right next to each other.  If so, the user may have typo'd "x =+ 4"
    // instead of "x += 4".
    Expr *RHSCheck = RHS;
    if (ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(RHSCheck))
      RHSCheck = ICE->getSubExpr();
    if (UnaryOperator *UO = dyn_cast<UnaryOperator>(RHSCheck)) {
      if ((UO->getOpcode() == UnaryOperator::Plus ||
           UO->getOpcode() == UnaryOperator::Minus) &&
          Loc.isFileID() && UO->getOperatorLoc().isFileID() &&
          // Only if the two operators are exactly adjacent.
          Loc.getFileLocWithOffset(1) == UO->getOperatorLoc() &&
          // And there is a space or other character before the subexpr of the
          // unary +/-.  We don't want to warn on "x=-1".
          Loc.getFileLocWithOffset(2) != UO->getSubExpr()->getLocStart() &&
          UO->getSubExpr()->getLocStart().isFileID()) {
        Diag(Loc, diag::warn_not_compound_assign)
          << (UO->getOpcode() == UnaryOperator::Plus ? "+" : "-")
          << SourceRange(UO->getOperatorLoc(), UO->getOperatorLoc());
      }
    }
  } else {
    // Compound assignment "x += y"
    ConvTy = CheckCompoundAssignmentConstraints(LHSType, RHSType);
  }

  if (DiagnoseAssignmentResult(ConvTy, Loc, LHSType, RHSType,
                               RHS, "assigning"))
    return QualType();

  // C99 6.5.16p3: The type of an assignment expression is the type of the
  // left operand unless the left operand has qualified type, in which case
  // it is the unqualified version of the type of the left operand.
  // C99 6.5.16.1p2: In simple assignment, the value of the right operand
  // is converted to the type of the assignment expression (above).
  // C++ 5.17p1: the type of the assignment expression is that of its left
  // oprdu.
  return LHSType.getUnqualifiedType();
}

// C99 6.5.17
QualType Sema::CheckCommaOperands(Expr *LHS, Expr *&RHS, SourceLocation Loc) {
  // FIXME: what is required for LHS?

  // Comma performs lvalue conversion (C99 6.3.2.1), but not unary conversions.
  DefaultFunctionArrayConversion(RHS);
  return RHS->getType();
}

/// CheckIncrementDecrementOperand - unlike most "Check" methods, this routine
/// doesn't need to call UsualUnaryConversions or UsualArithmeticConversions.
QualType Sema::CheckIncrementDecrementOperand(Expr *Op, SourceLocation OpLoc,
                                              bool isInc) {
  if (Op->isTypeDependent())
    return Context.DependentTy;

  QualType ResType = Op->getType();
  assert(!ResType.isNull() && "no type for increment/decrement expression");

  if (getLangOptions().CPlusPlus && ResType->isBooleanType()) {
    // Decrement of bool is not allowed.
    if (!isInc) {
      Diag(OpLoc, diag::err_decrement_bool) << Op->getSourceRange();
      return QualType();
    }
    // Increment of bool sets it to true, but is deprecated.
    Diag(OpLoc, diag::warn_increment_bool) << Op->getSourceRange();
  } else if (ResType->isRealType()) {
    // OK!
  } else if (const PointerType *PT = ResType->getAsPointerType()) {
    // C99 6.5.2.4p2, 6.5.6p2
    if (PT->getPointeeType()->isObjectType()) {
      // Pointer to object is ok!
    } else if (PT->getPointeeType()->isVoidType()) {
      if (getLangOptions().CPlusPlus) {
        Diag(OpLoc, diag::err_typecheck_pointer_arith_void_type)
          << Op->getSourceRange();
        return QualType();
      }

      // Pointer to void is a GNU extension in C.
      Diag(OpLoc, diag::ext_gnu_void_ptr) << Op->getSourceRange();
    } else if (PT->getPointeeType()->isFunctionType()) {
      if (getLangOptions().CPlusPlus) {
        Diag(OpLoc, diag::err_typecheck_pointer_arith_function_type)
          << Op->getType() << Op->getSourceRange();
        return QualType();
      }

      Diag(OpLoc, diag::ext_gnu_ptr_func_arith)
        << ResType << Op->getSourceRange();
    } else {
      RequireCompleteType(OpLoc, PT->getPointeeType(),
                             diag::err_typecheck_arithmetic_incomplete_type,
                             Op->getSourceRange(), SourceRange(),
                             ResType);
      return QualType();
    }
  } else if (ResType->isComplexType()) {
    // C99 does not support ++/-- on complex types, we allow as an extension.
    Diag(OpLoc, diag::ext_integer_increment_complex)
      << ResType << Op->getSourceRange();
  } else {
    Diag(OpLoc, diag::err_typecheck_illegal_increment_decrement)
      << ResType << Op->getSourceRange();
    return QualType();
  }
  // At this point, we know we have a real, complex or pointer type.
  // Now make sure the operand is a modifiable lvalue.
  if (CheckForModifiableLvalue(Op, OpLoc, *this))
    return QualType();
  return ResType;
}

/// getPrimaryDecl - Helper function for CheckAddressOfOperand().
/// This routine allows us to typecheck complex/recursive expressions
/// where the declaration is needed for type checking. We only need to
/// handle cases when the expression references a function designator
/// or is an lvalue. Here are some examples:
///  - &(x) => x
///  - &*****f => f for f a function designator.
///  - &s.xx => s
///  - &s.zz[1].yy -> s, if zz is an array
///  - *(x + 1) -> x, if x is an array
///  - &"123"[2] -> 0
///  - & __real__ x -> x
static NamedDecl *getPrimaryDecl(Expr *E) {
  switch (E->getStmtClass()) {
  case Stmt::DeclRefExprClass:
  case Stmt::QualifiedDeclRefExprClass:
    return cast<DeclRefExpr>(E)->getDecl();
  case Stmt::MemberExprClass:
    // Fields cannot be declared with a 'register' storage class.
    // &X->f is always ok, even if X is declared register.
    if (cast<MemberExpr>(E)->isArrow())
      return 0;
    return getPrimaryDecl(cast<MemberExpr>(E)->getBase());
  case Stmt::ArraySubscriptExprClass: {
    // &X[4] and &4[X] refers to X if X is not a pointer.

    NamedDecl *D = getPrimaryDecl(cast<ArraySubscriptExpr>(E)->getBase());
    ValueDecl *VD = dyn_cast_or_null<ValueDecl>(D);
    if (!VD || VD->getType()->isPointerType())
      return 0;
    else
      return VD;
  }
  case Stmt::UnaryOperatorClass: {
    UnaryOperator *UO = cast<UnaryOperator>(E);

    switch(UO->getOpcode()) {
    case UnaryOperator::Deref: {
      // *(X + 1) refers to X if X is not a pointer.
      if (NamedDecl *D = getPrimaryDecl(UO->getSubExpr())) {
        ValueDecl *VD = dyn_cast<ValueDecl>(D);
        if (!VD || VD->getType()->isPointerType())
          return 0;
        return VD;
      }
      return 0;
    }
    case UnaryOperator::Real:
    case UnaryOperator::Imag:
    case UnaryOperator::Extension:
      return getPrimaryDecl(UO->getSubExpr());
    default:
      return 0;
    }
  }
  case Stmt::BinaryOperatorClass: {
    BinaryOperator *BO = cast<BinaryOperator>(E);

    // Handle cases involving pointer arithmetic. The result of an
    // Assign or AddAssign is not an lvalue so they can be ignored.

    // (x + n) or (n + x) => x
    if (BO->getOpcode() == BinaryOperator::Add) {
      if (BO->getLHS()->getType()->isPointerType()) {
        return getPrimaryDecl(BO->getLHS());
      } else if (BO->getRHS()->getType()->isPointerType()) {
        return getPrimaryDecl(BO->getRHS());
      }
    }

    return 0;
  }
  case Stmt::ParenExprClass:
    return getPrimaryDecl(cast<ParenExpr>(E)->getSubExpr());
  case Stmt::ImplicitCastExprClass:
    // &X[4] when X is an array, has an implicit cast from array to pointer.
    return getPrimaryDecl(cast<ImplicitCastExpr>(E)->getSubExpr());
  default:
    return 0;
  }
}

/// CheckAddressOfOperand - The operand of & must be either a function
/// designator or an lvalue designating an object. If it is an lvalue, the
/// object cannot be declared with storage class register or be a bit field.
/// Note: The usual conversions are *not* applied to the operand of the &
/// operator (C99 6.3.2.1p[2-4]), and its result is never an lvalue.
/// In C++, the operand might be an overloaded function name, in which case
/// we allow the '&' but retain the overloaded-function type.
QualType Sema::CheckAddressOfOperand(Expr *op, SourceLocation OpLoc) {
  if (op->isTypeDependent())
    return Context.DependentTy;

  if (getLangOptions().C99) {
    // Implement C99-only parts of addressof rules.
    if (UnaryOperator* uOp = dyn_cast<UnaryOperator>(op)) {
      if (uOp->getOpcode() == UnaryOperator::Deref)
        // Per C99 6.5.3.2, the address of a deref always returns a valid result
        // (assuming the deref expression is valid).
        return uOp->getSubExpr()->getType();
    }
    // Technically, there should be a check for array subscript
    // expressions here, but the result of one is always an lvalue anyway.
  }
  NamedDecl *dcl = getPrimaryDecl(op);
  Expr::isLvalueResult lval = op->isLvalue(Context);

  if (lval != Expr::LV_Valid) { // C99 6.5.3.2p1
    if (!dcl || !isa<FunctionDecl>(dcl)) {// allow function designators
      // FIXME: emit more specific diag...
      Diag(OpLoc, diag::err_typecheck_invalid_lvalue_addrof)
        << op->getSourceRange();
      return QualType();
    }
  } else if (MemberExpr *MemExpr = dyn_cast<MemberExpr>(op)) { // C99 6.5.3.2p1
    if (FieldDecl *Field = dyn_cast<FieldDecl>(MemExpr->getMemberDecl())) {
      if (Field->isBitField()) {
        Diag(OpLoc, diag::err_typecheck_address_of)
          << "bit-field" << op->getSourceRange();
        return QualType();
      }
    }
  // Check for Apple extension for accessing vector components.
  } else if (isa<ExtVectorElementExpr>(op) || (isa<ArraySubscriptExpr>(op) &&
           cast<ArraySubscriptExpr>(op)->getBase()->getType()->isVectorType())){
    Diag(OpLoc, diag::err_typecheck_address_of)
      << "vector element" << op->getSourceRange();
    return QualType();
  } else if (dcl) { // C99 6.5.3.2p1
    // We have an lvalue with a decl. Make sure the decl is not declared
    // with the register storage-class specifier.
    if (const VarDecl *vd = dyn_cast<VarDecl>(dcl)) {
      if (vd->getStorageClass() == VarDecl::Register) {
        Diag(OpLoc, diag::err_typecheck_address_of)
          << "register variable" << op->getSourceRange();
        return QualType();
      }
    } else if (isa<OverloadedFunctionDecl>(dcl)) {
      return Context.OverloadTy;
    } else if (isa<FieldDecl>(dcl)) {
      // Okay: we can take the address of a field.
      // Could be a pointer to member, though, if there is an explicit
      // scope qualifier for the class.
      if (isa<QualifiedDeclRefExpr>(op)) {
        DeclContext *Ctx = dcl->getDeclContext();
        if (Ctx && Ctx->isRecord())
          return Context.getMemberPointerType(op->getType(),
                Context.getTypeDeclType(cast<RecordDecl>(Ctx)).getTypePtr());
      }
    } else if (isa<FunctionDecl>(dcl)) {
      // Okay: we can take the address of a function.
      // As above.
      if (isa<QualifiedDeclRefExpr>(op)) {
        DeclContext *Ctx = dcl->getDeclContext();
        if (Ctx && Ctx->isRecord())
          return Context.getMemberPointerType(op->getType(),
                Context.getTypeDeclType(cast<RecordDecl>(Ctx)).getTypePtr());
      }
    }
    else
      assert(0 && "Unknown/unexpected decl type");
  }

  // If the operand has type "type", the result has type "pointer to type".
  return Context.getPointerType(op->getType());
}

QualType Sema::CheckIndirectionOperand(Expr *Op, SourceLocation OpLoc) {
  if (Op->isTypeDependent())
    return Context.DependentTy;

  UsualUnaryConversions(Op);
  QualType Ty = Op->getType();

  // Note that per both C89 and C99, this is always legal, even if ptype is an
  // incomplete type or void.  It would be possible to warn about dereferencing
  // a void pointer, but it's completely well-defined, and such a warning is
  // unlikely to catch any mistakes.
  if (const PointerType *PT = Ty->getAsPointerType())
    return PT->getPointeeType();

  Diag(OpLoc, diag::err_typecheck_indirection_requires_pointer)
    << Ty << Op->getSourceRange();
  return QualType();
}

static inline BinaryOperator::Opcode ConvertTokenKindToBinaryOpcode(
  tok::TokenKind Kind) {
  BinaryOperator::Opcode Opc;
  switch (Kind) {
  default: assert(0 && "Unknown binop!");
  case tok::periodstar:           Opc = BinaryOperator::PtrMemD; break;
  case tok::arrowstar:            Opc = BinaryOperator::PtrMemI; break;
  case tok::star:                 Opc = BinaryOperator::Mul; break;
  case tok::slash:                Opc = BinaryOperator::Div; break;
  case tok::percent:              Opc = BinaryOperator::Rem; break;
  case tok::plus:                 Opc = BinaryOperator::Add; break;
  case tok::minus:                Opc = BinaryOperator::Sub; break;
  case tok::lessless:             Opc = BinaryOperator::Shl; break;
  case tok::greatergreater:       Opc = BinaryOperator::Shr; break;
  case tok::lessequal:            Opc = BinaryOperator::LE; break;
  case tok::less:                 Opc = BinaryOperator::LT; break;
  case tok::greaterequal:         Opc = BinaryOperator::GE; break;
  case tok::greater:              Opc = BinaryOperator::GT; break;
  case tok::exclaimequal:         Opc = BinaryOperator::NE; break;
  case tok::equalequal:           Opc = BinaryOperator::EQ; break;
  case tok::amp:                  Opc = BinaryOperator::And; break;
  case tok::caret:                Opc = BinaryOperator::Xor; break;
  case tok::pipe:                 Opc = BinaryOperator::Or; break;
  case tok::ampamp:               Opc = BinaryOperator::LAnd; break;
  case tok::pipepipe:             Opc = BinaryOperator::LOr; break;
  case tok::equal:                Opc = BinaryOperator::Assign; break;
  case tok::starequal:            Opc = BinaryOperator::MulAssign; break;
  case tok::slashequal:           Opc = BinaryOperator::DivAssign; break;
  case tok::percentequal:         Opc = BinaryOperator::RemAssign; break;
  case tok::plusequal:            Opc = BinaryOperator::AddAssign; break;
  case tok::minusequal:           Opc = BinaryOperator::SubAssign; break;
  case tok::lesslessequal:        Opc = BinaryOperator::ShlAssign; break;
  case tok::greatergreaterequal:  Opc = BinaryOperator::ShrAssign; break;
  case tok::ampequal:             Opc = BinaryOperator::AndAssign; break;
  case tok::caretequal:           Opc = BinaryOperator::XorAssign; break;
  case tok::pipeequal:            Opc = BinaryOperator::OrAssign; break;
  case tok::comma:                Opc = BinaryOperator::Comma; break;
  }
  return Opc;
}

static inline UnaryOperator::Opcode ConvertTokenKindToUnaryOpcode(
  tok::TokenKind Kind) {
  UnaryOperator::Opcode Opc;
  switch (Kind) {
  default: assert(0 && "Unknown unary op!");
  case tok::plusplus:     Opc = UnaryOperator::PreInc; break;
  case tok::minusminus:   Opc = UnaryOperator::PreDec; break;
  case tok::amp:          Opc = UnaryOperator::AddrOf; break;
  case tok::star:         Opc = UnaryOperator::Deref; break;
  case tok::plus:         Opc = UnaryOperator::Plus; break;
  case tok::minus:        Opc = UnaryOperator::Minus; break;
  case tok::tilde:        Opc = UnaryOperator::Not; break;
  case tok::exclaim:      Opc = UnaryOperator::LNot; break;
  case tok::kw___real:    Opc = UnaryOperator::Real; break;
  case tok::kw___imag:    Opc = UnaryOperator::Imag; break;
  case tok::kw___extension__: Opc = UnaryOperator::Extension; break;
  }
  return Opc;
}

/// CreateBuiltinBinOp - Creates a new built-in binary operation with
/// operator @p Opc at location @c TokLoc. This routine only supports
/// built-in operations; ActOnBinOp handles overloaded operators.
Action::OwningExprResult Sema::CreateBuiltinBinOp(SourceLocation OpLoc,
                                                  unsigned Op,
                                                  Expr *lhs, Expr *rhs) {
  QualType ResultTy;  // Result type of the binary operator.
  QualType CompTy;    // Computation type for compound assignments (e.g. '+=')
  BinaryOperator::Opcode Opc = (BinaryOperator::Opcode)Op;

  switch (Opc) {
  default:
    assert(0 && "Unknown binary expr!");
  case BinaryOperator::Assign:
    ResultTy = CheckAssignmentOperands(lhs, rhs, OpLoc, QualType());
    break;
  case BinaryOperator::PtrMemD:
  case BinaryOperator::PtrMemI:
    ResultTy = CheckPointerToMemberOperands(lhs, rhs, OpLoc,
                                            Opc == BinaryOperator::PtrMemI);
    break;
  case BinaryOperator::Mul:
  case BinaryOperator::Div:
    ResultTy = CheckMultiplyDivideOperands(lhs, rhs, OpLoc);
    break;
  case BinaryOperator::Rem:
    ResultTy = CheckRemainderOperands(lhs, rhs, OpLoc);
    break;
  case BinaryOperator::Add:
    ResultTy = CheckAdditionOperands(lhs, rhs, OpLoc);
    break;
  case BinaryOperator::Sub:
    ResultTy = CheckSubtractionOperands(lhs, rhs, OpLoc);
    break;
  case BinaryOperator::Shl:
  case BinaryOperator::Shr:
    ResultTy = CheckShiftOperands(lhs, rhs, OpLoc);
    break;
  case BinaryOperator::LE:
  case BinaryOperator::LT:
  case BinaryOperator::GE:
  case BinaryOperator::GT:
    ResultTy = CheckCompareOperands(lhs, rhs, OpLoc, true);
    break;
  case BinaryOperator::EQ:
  case BinaryOperator::NE:
    ResultTy = CheckCompareOperands(lhs, rhs, OpLoc, false);
    break;
  case BinaryOperator::And:
  case BinaryOperator::Xor:
  case BinaryOperator::Or:
    ResultTy = CheckBitwiseOperands(lhs, rhs, OpLoc);
    break;
  case BinaryOperator::LAnd:
  case BinaryOperator::LOr:
    ResultTy = CheckLogicalOperands(lhs, rhs, OpLoc);
    break;
  case BinaryOperator::MulAssign:
  case BinaryOperator::DivAssign:
    CompTy = CheckMultiplyDivideOperands(lhs, rhs, OpLoc, true);
    if (!CompTy.isNull())
      ResultTy = CheckAssignmentOperands(lhs, rhs, OpLoc, CompTy);
    break;
  case BinaryOperator::RemAssign:
    CompTy = CheckRemainderOperands(lhs, rhs, OpLoc, true);
    if (!CompTy.isNull())
      ResultTy = CheckAssignmentOperands(lhs, rhs, OpLoc, CompTy);
    break;
  case BinaryOperator::AddAssign:
    CompTy = CheckAdditionOperands(lhs, rhs, OpLoc, true);
    if (!CompTy.isNull())
      ResultTy = CheckAssignmentOperands(lhs, rhs, OpLoc, CompTy);
    break;
  case BinaryOperator::SubAssign:
    CompTy = CheckSubtractionOperands(lhs, rhs, OpLoc, true);
    if (!CompTy.isNull())
      ResultTy = CheckAssignmentOperands(lhs, rhs, OpLoc, CompTy);
    break;
  case BinaryOperator::ShlAssign:
  case BinaryOperator::ShrAssign:
    CompTy = CheckShiftOperands(lhs, rhs, OpLoc, true);
    if (!CompTy.isNull())
      ResultTy = CheckAssignmentOperands(lhs, rhs, OpLoc, CompTy);
    break;
  case BinaryOperator::AndAssign:
  case BinaryOperator::XorAssign:
  case BinaryOperator::OrAssign:
    CompTy = CheckBitwiseOperands(lhs, rhs, OpLoc, true);
    if (!CompTy.isNull())
      ResultTy = CheckAssignmentOperands(lhs, rhs, OpLoc, CompTy);
    break;
  case BinaryOperator::Comma:
    ResultTy = CheckCommaOperands(lhs, rhs, OpLoc);
    break;
  }
  if (ResultTy.isNull())
    return ExprError();
  if (CompTy.isNull())
    return Owned(new (Context) BinaryOperator(lhs, rhs, Opc, ResultTy, OpLoc));
  else
    return Owned(new (Context) CompoundAssignOperator(lhs, rhs, Opc, ResultTy,
                                                      CompTy, OpLoc));
}

// Binary Operators.  'Tok' is the token for the operator.
Action::OwningExprResult Sema::ActOnBinOp(Scope *S, SourceLocation TokLoc,
                                          tok::TokenKind Kind,
                                          ExprArg LHS, ExprArg RHS) {
  BinaryOperator::Opcode Opc = ConvertTokenKindToBinaryOpcode(Kind);
  Expr *lhs = (Expr *)LHS.release(), *rhs = (Expr*)RHS.release();

  assert((lhs != 0) && "ActOnBinOp(): missing left expression");
  assert((rhs != 0) && "ActOnBinOp(): missing right expression");

  // If either expression is type-dependent, just build the AST.
  // FIXME: We'll need to perform some caching of the result of name
  // lookup for operator+.
  if (lhs->isTypeDependent() || rhs->isTypeDependent()) {
    if (Opc > BinaryOperator::Assign && Opc <= BinaryOperator::OrAssign)
      return Owned(new (Context) CompoundAssignOperator(lhs, rhs, Opc,
                                              Context.DependentTy,
                                              Context.DependentTy, TokLoc));
    else
      return Owned(new (Context) BinaryOperator(lhs, rhs, Opc,
                                                Context.DependentTy, TokLoc));
  }

  if (getLangOptions().CPlusPlus && Opc != BinaryOperator::PtrMemD &&
      (lhs->getType()->isRecordType() || lhs->getType()->isEnumeralType() ||
       rhs->getType()->isRecordType() || rhs->getType()->isEnumeralType())) {
    // If this is one of the assignment operators, we only perform
    // overload resolution if the left-hand side is a class or
    // enumeration type (C++ [expr.ass]p3).
    if (Opc >= BinaryOperator::Assign && Opc <= BinaryOperator::OrAssign &&
        !(lhs->getType()->isRecordType() || lhs->getType()->isEnumeralType())) {
      return CreateBuiltinBinOp(TokLoc, Opc, lhs, rhs);
    }

    // Determine which overloaded operator we're dealing with.
    static const OverloadedOperatorKind OverOps[] = {
      // Overloading .* is not possible.
      static_cast<OverloadedOperatorKind>(0), OO_ArrowStar,
      OO_Star, OO_Slash, OO_Percent,
      OO_Plus, OO_Minus,
      OO_LessLess, OO_GreaterGreater,
      OO_Less, OO_Greater, OO_LessEqual, OO_GreaterEqual,
      OO_EqualEqual, OO_ExclaimEqual,
      OO_Amp,
      OO_Caret,
      OO_Pipe,
      OO_AmpAmp,
      OO_PipePipe,
      OO_Equal, OO_StarEqual,
      OO_SlashEqual, OO_PercentEqual,
      OO_PlusEqual, OO_MinusEqual,
      OO_LessLessEqual, OO_GreaterGreaterEqual,
      OO_AmpEqual, OO_CaretEqual,
      OO_PipeEqual,
      OO_Comma
    };
    OverloadedOperatorKind OverOp = OverOps[Opc];

    // Add the appropriate overloaded operators (C++ [over.match.oper])
    // to the candidate set.
    OverloadCandidateSet CandidateSet;
    Expr *Args[2] = { lhs, rhs };
    if (AddOperatorCandidates(OverOp, S, TokLoc, Args, 2, CandidateSet))
      return ExprError();

    // Perform overload resolution.
    OverloadCandidateSet::iterator Best;
    switch (BestViableFunction(CandidateSet, Best)) {
    case OR_Success: {
      // We found a built-in operator or an overloaded operator.
      FunctionDecl *FnDecl = Best->Function;

      if (FnDecl) {
        // We matched an overloaded operator. Build a call to that
        // operator.

        // Convert the arguments.
        if (CXXMethodDecl *Method = dyn_cast<CXXMethodDecl>(FnDecl)) {
          if (PerformObjectArgumentInitialization(lhs, Method) ||
              PerformCopyInitialization(rhs, FnDecl->getParamDecl(0)->getType(),
                                        "passing"))
            return ExprError();
        } else {
          // Convert the arguments.
          if (PerformCopyInitialization(lhs, FnDecl->getParamDecl(0)->getType(),
                                        "passing") ||
              PerformCopyInitialization(rhs, FnDecl->getParamDecl(1)->getType(),
                                        "passing"))
            return ExprError();
        }

        // Determine the result type
        QualType ResultTy
          = FnDecl->getType()->getAsFunctionType()->getResultType();
        ResultTy = ResultTy.getNonReferenceType();

        // Build the actual expression node.
        Expr *FnExpr = new (Context) DeclRefExpr(FnDecl, FnDecl->getType(),
                                                 SourceLocation());
        UsualUnaryConversions(FnExpr);

        return Owned(new (Context) CXXOperatorCallExpr(Context, FnExpr, Args, 2,
                                                       ResultTy, TokLoc));
      } else {
        // We matched a built-in operator. Convert the arguments, then
        // break out so that we will build the appropriate built-in
        // operator node.
        if (PerformImplicitConversion(lhs, Best->BuiltinTypes.ParamTypes[0],
                                      Best->Conversions[0], "passing") ||
            PerformImplicitConversion(rhs, Best->BuiltinTypes.ParamTypes[1],
                                      Best->Conversions[1], "passing"))
          return ExprError();

        break;
      }
    }

    case OR_No_Viable_Function:
      // No viable function; fall through to handling this as a
      // built-in operator, which will produce an error message for us.
      break;

    case OR_Ambiguous:
      Diag(TokLoc,  diag::err_ovl_ambiguous_oper)
          << BinaryOperator::getOpcodeStr(Opc)
          << lhs->getSourceRange() << rhs->getSourceRange();
      PrintOverloadCandidates(CandidateSet, /*OnlyViable=*/true);
      return ExprError();

    case OR_Deleted:
      Diag(TokLoc, diag::err_ovl_deleted_oper)
        << Best->Function->isDeleted()
        << BinaryOperator::getOpcodeStr(Opc)
        << lhs->getSourceRange() << rhs->getSourceRange();
      PrintOverloadCandidates(CandidateSet, /*OnlyViable=*/true);
      return ExprError();
    }

    // Either we found no viable overloaded operator or we matched a
    // built-in operator. In either case, fall through to trying to
    // build a built-in operation.
  }

  // Build a built-in binary operation.
  return CreateBuiltinBinOp(TokLoc, Opc, lhs, rhs);
}

// Unary Operators.  'Tok' is the token for the operator.
Action::OwningExprResult Sema::ActOnUnaryOp(Scope *S, SourceLocation OpLoc,
                                            tok::TokenKind Op, ExprArg input) {
  // FIXME: Input is modified later, but smart pointer not reassigned.
  Expr *Input = (Expr*)input.get();
  UnaryOperator::Opcode Opc = ConvertTokenKindToUnaryOpcode(Op);

  if (getLangOptions().CPlusPlus &&
      (Input->getType()->isRecordType()
       || Input->getType()->isEnumeralType())) {
    // Determine which overloaded operator we're dealing with.
    static const OverloadedOperatorKind OverOps[] = {
      OO_None, OO_None,
      OO_PlusPlus, OO_MinusMinus,
      OO_Amp, OO_Star,
      OO_Plus, OO_Minus,
      OO_Tilde, OO_Exclaim,
      OO_None, OO_None,
      OO_None,
      OO_None
    };
    OverloadedOperatorKind OverOp = OverOps[Opc];

    // Add the appropriate overloaded operators (C++ [over.match.oper])
    // to the candidate set.
    OverloadCandidateSet CandidateSet;
    if (OverOp != OO_None &&
        AddOperatorCandidates(OverOp, S, OpLoc, &Input, 1, CandidateSet))
      return ExprError();

    // Perform overload resolution.
    OverloadCandidateSet::iterator Best;
    switch (BestViableFunction(CandidateSet, Best)) {
    case OR_Success: {
      // We found a built-in operator or an overloaded operator.
      FunctionDecl *FnDecl = Best->Function;

      if (FnDecl) {
        // We matched an overloaded operator. Build a call to that
        // operator.

        // Convert the arguments.
        if (CXXMethodDecl *Method = dyn_cast<CXXMethodDecl>(FnDecl)) {
          if (PerformObjectArgumentInitialization(Input, Method))
            return ExprError();
        } else {
          // Convert the arguments.
          if (PerformCopyInitialization(Input,
                                        FnDecl->getParamDecl(0)->getType(),
                                        "passing"))
            return ExprError();
        }

        // Determine the result type
        QualType ResultTy
          = FnDecl->getType()->getAsFunctionType()->getResultType();
        ResultTy = ResultTy.getNonReferenceType();

        // Build the actual expression node.
        Expr *FnExpr = new (Context) DeclRefExpr(FnDecl, FnDecl->getType(),
                                                 SourceLocation());
        UsualUnaryConversions(FnExpr);

        input.release();
        return Owned(new (Context) CXXOperatorCallExpr(Context, FnExpr, &Input,
                                                       1, ResultTy, OpLoc));
      } else {
        // We matched a built-in operator. Convert the arguments, then
        // break out so that we will build the appropriate built-in
        // operator node.
        if (PerformImplicitConversion(Input, Best->BuiltinTypes.ParamTypes[0],
                                      Best->Conversions[0], "passing"))
          return ExprError();

        break;
      }
    }

    case OR_No_Viable_Function:
      // No viable function; fall through to handling this as a
      // built-in operator, which will produce an error message for us.
      break;

    case OR_Ambiguous:
      Diag(OpLoc,  diag::err_ovl_ambiguous_oper)
          << UnaryOperator::getOpcodeStr(Opc)
          << Input->getSourceRange();
      PrintOverloadCandidates(CandidateSet, /*OnlyViable=*/true);
      return ExprError();

    case OR_Deleted:
      Diag(OpLoc, diag::err_ovl_deleted_oper)
        << Best->Function->isDeleted()
        << UnaryOperator::getOpcodeStr(Opc)
        << Input->getSourceRange();
      PrintOverloadCandidates(CandidateSet, /*OnlyViable=*/true);
      return ExprError();
    }

    // Either we found no viable overloaded operator or we matched a
    // built-in operator. In either case, fall through to trying to
    // build a built-in operation.
  }

  QualType resultType;
  switch (Opc) {
  default:
    assert(0 && "Unimplemented unary expr!");
  case UnaryOperator::PreInc:
  case UnaryOperator::PreDec:
    resultType = CheckIncrementDecrementOperand(Input, OpLoc,
                                                Opc == UnaryOperator::PreInc);
    break;
  case UnaryOperator::AddrOf:
    resultType = CheckAddressOfOperand(Input, OpLoc);
    break;
  case UnaryOperator::Deref:
    DefaultFunctionArrayConversion(Input);
    resultType = CheckIndirectionOperand(Input, OpLoc);
    break;
  case UnaryOperator::Plus:
  case UnaryOperator::Minus:
    UsualUnaryConversions(Input);
    resultType = Input->getType();
    if (resultType->isDependentType())
      break;
    if (resultType->isArithmeticType()) // C99 6.5.3.3p1
      break;
    else if (getLangOptions().CPlusPlus && // C++ [expr.unary.op]p6-7
             resultType->isEnumeralType())
      break;
    else if (getLangOptions().CPlusPlus && // C++ [expr.unary.op]p6
             Opc == UnaryOperator::Plus &&
             resultType->isPointerType())
      break;

    return ExprError(Diag(OpLoc, diag::err_typecheck_unary_expr)
      << resultType << Input->getSourceRange());
  case UnaryOperator::Not: // bitwise complement
    UsualUnaryConversions(Input);
    resultType = Input->getType();
    if (resultType->isDependentType())
      break;
    // C99 6.5.3.3p1. We allow complex int and float as a GCC extension.
    if (resultType->isComplexType() || resultType->isComplexIntegerType())
      // C99 does not support '~' for complex conjugation.
      Diag(OpLoc, diag::ext_integer_complement_complex)
        << resultType << Input->getSourceRange();
    else if (!resultType->isIntegerType())
      return ExprError(Diag(OpLoc, diag::err_typecheck_unary_expr)
        << resultType << Input->getSourceRange());
    break;
  case UnaryOperator::LNot: // logical negation
    // Unlike +/-/~, integer promotions aren't done here (C99 6.5.3.3p5).
    DefaultFunctionArrayConversion(Input);
    resultType = Input->getType();
    if (resultType->isDependentType())
      break;
    if (!resultType->isScalarType()) // C99 6.5.3.3p1
      return ExprError(Diag(OpLoc, diag::err_typecheck_unary_expr)
        << resultType << Input->getSourceRange());
    // LNot always has type int. C99 6.5.3.3p5.
    // In C++, it's bool. C++ 5.3.1p8
    resultType = getLangOptions().CPlusPlus ? Context.BoolTy : Context.IntTy;
    break;
  case UnaryOperator::Real:
  case UnaryOperator::Imag:
    resultType = CheckRealImagOperand(Input, OpLoc, Opc == UnaryOperator::Real);
    break;
  case UnaryOperator::Extension:
    resultType = Input->getType();
    break;
  }
  if (resultType.isNull())
    return ExprError();
  input.release();
  return Owned(new (Context) UnaryOperator(Input, Opc, resultType, OpLoc));
}

/// ActOnAddrLabel - Parse the GNU address of label extension: "&&foo".
Sema::ExprResult Sema::ActOnAddrLabel(SourceLocation OpLoc,
                                      SourceLocation LabLoc,
                                      IdentifierInfo *LabelII) {
  // Look up the record for this label identifier.
  llvm::DenseMap<IdentifierInfo*, Action::StmtTy*>::iterator I = 
    ActiveScope->LabelMap.find(LabelII);

  LabelStmt *LabelDecl;
  
  // If we haven't seen this label yet, create a forward reference. It
  // will be validated and/or cleaned up in ActOnFinishFunctionBody.
  if (I == ActiveScope->LabelMap.end()) {
    LabelDecl = new (Context) LabelStmt(LabLoc, LabelII, 0);

    ActiveScope->LabelMap.insert(std::make_pair(LabelII, LabelDecl));
  } else
    LabelDecl = static_cast<LabelStmt *>(I->second);
    
  // Create the AST node.  The address of a label always has type 'void*'.
  return new (Context) AddrLabelExpr(OpLoc, LabLoc, LabelDecl,
                                     Context.getPointerType(Context.VoidTy));
}

Sema::ExprResult Sema::ActOnStmtExpr(SourceLocation LPLoc, StmtTy *substmt,
                                     SourceLocation RPLoc) { // "({..})"
  Stmt *SubStmt = static_cast<Stmt*>(substmt);
  assert(SubStmt && isa<CompoundStmt>(SubStmt) && "Invalid action invocation!");
  CompoundStmt *Compound = cast<CompoundStmt>(SubStmt);

  bool isFileScope = getCurFunctionOrMethodDecl() == 0;
  if (isFileScope) {
    return Diag(LPLoc, diag::err_stmtexpr_file_scope);
  }

  // FIXME: there are a variety of strange constraints to enforce here, for
  // example, it is not possible to goto into a stmt expression apparently.
  // More semantic analysis is needed.

  // FIXME: the last statement in the compount stmt has its value used.  We
  // should not warn about it being unused.

  // If there are sub stmts in the compound stmt, take the type of the last one
  // as the type of the stmtexpr.
  QualType Ty = Context.VoidTy;

  if (!Compound->body_empty()) {
    Stmt *LastStmt = Compound->body_back();
    // If LastStmt is a label, skip down through into the body.
    while (LabelStmt *Label = dyn_cast<LabelStmt>(LastStmt))
      LastStmt = Label->getSubStmt();

    if (Expr *LastExpr = dyn_cast<Expr>(LastStmt))
      Ty = LastExpr->getType();
  }

  return new (Context) StmtExpr(Compound, Ty, LPLoc, RPLoc);
}

Sema::ExprResult Sema::ActOnBuiltinOffsetOf(Scope *S,
                                            SourceLocation BuiltinLoc,
                                            SourceLocation TypeLoc,
                                            TypeTy *argty,
                                            OffsetOfComponent *CompPtr,
                                            unsigned NumComponents,
                                            SourceLocation RPLoc) {
  QualType ArgTy = QualType::getFromOpaquePtr(argty);
  assert(!ArgTy.isNull() && "Missing type argument!");

  bool Dependent = ArgTy->isDependentType();

  // We must have at least one component that refers to the type, and the first
  // one is known to be a field designator.  Verify that the ArgTy represents
  // a struct/union/class.
  if (!Dependent && !ArgTy->isRecordType())
    return Diag(TypeLoc, diag::err_offsetof_record_type) << ArgTy;

  // FIXME: Does the type need to be complete?

  // Otherwise, create a null pointer as the base, and iteratively process
  // the offsetof designators.
  QualType ArgTyPtr = Context.getPointerType(ArgTy);
  Expr* Res = new (Context) ImplicitValueInitExpr(ArgTyPtr);
  Res = new (Context) UnaryOperator(Res, UnaryOperator::Deref, 
                                    ArgTy, SourceLocation());

  // offsetof with non-identifier designators (e.g. "offsetof(x, a.b[c])") are a
  // GCC extension, diagnose them.
  // FIXME: This diagnostic isn't actually visible because the location is in
  // a system header!
  if (NumComponents != 1)
    Diag(BuiltinLoc, diag::ext_offsetof_extended_field_designator)
      << SourceRange(CompPtr[1].LocStart, CompPtr[NumComponents-1].LocEnd);

  if (!Dependent) {
    // FIXME: Dependent case loses a lot of information here. And probably
    // leaks like a sieve.
    for (unsigned i = 0; i != NumComponents; ++i) {
      const OffsetOfComponent &OC = CompPtr[i];
      if (OC.isBrackets) {
        // Offset of an array sub-field.  TODO: Should we allow vector elements?
        const ArrayType *AT = Context.getAsArrayType(Res->getType());
        if (!AT) {
          Res->Destroy(Context);
          return Diag(OC.LocEnd, diag::err_offsetof_array_type)
            << Res->getType();
        }

        // FIXME: C++: Verify that operator[] isn't overloaded.

        // Promote the array so it looks more like a normal array subscript
        // expression.
        DefaultFunctionArrayConversion(Res);

        // C99 6.5.2.1p1
        Expr *Idx = static_cast<Expr*>(OC.U.E);
        if (!Idx->isTypeDependent() && !Idx->getType()->isIntegerType())
          return Diag(Idx->getLocStart(), diag::err_typecheck_subscript)
            << Idx->getSourceRange();

        Res = new (Context) ArraySubscriptExpr(Res, Idx, AT->getElementType(),
                                               OC.LocEnd);
        continue;
      }

      const RecordType *RC = Res->getType()->getAsRecordType();
      if (!RC) {
        Res->Destroy(Context);
        return Diag(OC.LocEnd, diag::err_offsetof_record_type)
          << Res->getType();
      }

      // Get the decl corresponding to this.
      RecordDecl *RD = RC->getDecl();
      FieldDecl *MemberDecl
        = dyn_cast_or_null<FieldDecl>(LookupQualifiedName(RD, OC.U.IdentInfo,
                                                          LookupMemberName)
                                        .getAsDecl());
      if (!MemberDecl)
        return Diag(BuiltinLoc, diag::err_typecheck_no_member)
         << OC.U.IdentInfo << SourceRange(OC.LocStart, OC.LocEnd);

      // FIXME: C++: Verify that MemberDecl isn't a static field.
      // FIXME: Verify that MemberDecl isn't a bitfield.
      // MemberDecl->getType() doesn't get the right qualifiers, but it doesn't
      // matter here.
      Res = new (Context) MemberExpr(Res, false, MemberDecl, OC.LocEnd,
                                   MemberDecl->getType().getNonReferenceType());
    }
  }

  return new (Context) UnaryOperator(Res, UnaryOperator::OffsetOf,
                                     Context.getSizeType(), BuiltinLoc);
}


Sema::ExprResult Sema::ActOnTypesCompatibleExpr(SourceLocation BuiltinLoc,
                                                TypeTy *arg1, TypeTy *arg2,
                                                SourceLocation RPLoc) {
  QualType argT1 = QualType::getFromOpaquePtr(arg1);
  QualType argT2 = QualType::getFromOpaquePtr(arg2);

  assert((!argT1.isNull() && !argT2.isNull()) && "Missing type argument(s)");

  return new (Context) TypesCompatibleExpr(Context.IntTy, BuiltinLoc, argT1,
                                           argT2, RPLoc);
}

Sema::ExprResult Sema::ActOnChooseExpr(SourceLocation BuiltinLoc, ExprTy *cond,
                                       ExprTy *expr1, ExprTy *expr2,
                                       SourceLocation RPLoc) {
  Expr *CondExpr = static_cast<Expr*>(cond);
  Expr *LHSExpr = static_cast<Expr*>(expr1);
  Expr *RHSExpr = static_cast<Expr*>(expr2);

  assert((CondExpr && LHSExpr && RHSExpr) && "Missing type argument(s)");

  QualType resType;
  if (CondExpr->isValueDependent()) {
    resType = Context.DependentTy;
  } else {
    // The conditional expression is required to be a constant expression.
    llvm::APSInt condEval(32);
    SourceLocation ExpLoc;
    if (!CondExpr->isIntegerConstantExpr(condEval, Context, &ExpLoc))
      return Diag(ExpLoc, diag::err_typecheck_choose_expr_requires_constant)
        << CondExpr->getSourceRange();

    // If the condition is > zero, then the AST type is the same as the LSHExpr.
    resType = condEval.getZExtValue() ? LHSExpr->getType() : RHSExpr->getType();
  }

  return new (Context) ChooseExpr(BuiltinLoc, CondExpr, LHSExpr, RHSExpr,
                                  resType, RPLoc);
}

//===----------------------------------------------------------------------===//
// Clang Extensions.
//===----------------------------------------------------------------------===//

/// ActOnBlockStart - This callback is invoked when a block literal is started.
void Sema::ActOnBlockStart(SourceLocation CaretLoc, Scope *BlockScope) {
  // Analyze block parameters.
  BlockSemaInfo *BSI = new BlockSemaInfo();

  // Add BSI to CurBlock.
  BSI->PrevBlockInfo = CurBlock;
  CurBlock = BSI;
  ActiveScope = BlockScope;

  BSI->ReturnType = 0;
  BSI->TheScope = BlockScope;
  BSI->hasBlockDeclRefExprs = false;

  BSI->TheDecl = BlockDecl::Create(Context, CurContext, CaretLoc);
  PushDeclContext(BlockScope, BSI->TheDecl);
}

void Sema::ActOnBlockArguments(Declarator &ParamInfo, Scope *CurScope) {
  assert(ParamInfo.getIdentifier() == 0 && "block-id should have no identifier!");

  if (ParamInfo.getNumTypeObjects() == 0
      || ParamInfo.getTypeObject(0).Kind != DeclaratorChunk::Function) {
    QualType T = GetTypeForDeclarator(ParamInfo, CurScope);

    // The type is entirely optional as well, if none, use DependentTy.
    if (T.isNull())
      T = Context.DependentTy;

    // The parameter list is optional, if there was none, assume ().
    if (!T->isFunctionType())
      T = Context.getFunctionType(T, NULL, 0, 0, 0);

    CurBlock->hasPrototype = true;
    CurBlock->isVariadic = false;
    Type *RetTy = T.getTypePtr()->getAsFunctionType()->getResultType()
      .getTypePtr();

    if (!RetTy->isDependentType())
      CurBlock->ReturnType = RetTy;
    return;
  }

  // Analyze arguments to block.
  assert(ParamInfo.getTypeObject(0).Kind == DeclaratorChunk::Function &&
         "Not a function declarator!");
  DeclaratorChunk::FunctionTypeInfo &FTI = ParamInfo.getTypeObject(0).Fun;

  CurBlock->hasPrototype = FTI.hasPrototype;
  CurBlock->isVariadic = true;

  // Check for C99 6.7.5.3p10 - foo(void) is a non-varargs function that takes
  // no arguments, not a function that takes a single void argument.
  if (FTI.hasPrototype &&
      FTI.NumArgs == 1 && !FTI.isVariadic && FTI.ArgInfo[0].Ident == 0 &&
      (!((ParmVarDecl *)FTI.ArgInfo[0].Param)->getType().getCVRQualifiers() &&
        ((ParmVarDecl *)FTI.ArgInfo[0].Param)->getType()->isVoidType())) {
    // empty arg list, don't push any params.
    CurBlock->isVariadic = false;
  } else if (FTI.hasPrototype) {
    for (unsigned i = 0, e = FTI.NumArgs; i != e; ++i)
      CurBlock->Params.push_back((ParmVarDecl *)FTI.ArgInfo[i].Param);
    CurBlock->isVariadic = FTI.isVariadic;
    QualType T = GetTypeForDeclarator (ParamInfo, CurScope);

    Type* RetTy = T.getTypePtr()->getAsFunctionType()->getResultType()
      .getTypePtr();

    if (!RetTy->isDependentType())
      CurBlock->ReturnType = RetTy;
  }
  CurBlock->TheDecl->setArgs(&CurBlock->Params[0], CurBlock->Params.size());

  for (BlockDecl::param_iterator AI = CurBlock->TheDecl->param_begin(),
       E = CurBlock->TheDecl->param_end(); AI != E; ++AI)
    // If this has an identifier, add it to the scope stack.
    if ((*AI)->getIdentifier())
      PushOnScopeChains(*AI, CurBlock->TheScope);
}

/// ActOnBlockError - If there is an error parsing a block, this callback
/// is invoked to pop the information about the block from the action impl.
void Sema::ActOnBlockError(SourceLocation CaretLoc, Scope *CurScope) {
  // Ensure that CurBlock is deleted.
  llvm::OwningPtr<BlockSemaInfo> CC(CurBlock);

  // Pop off CurBlock, handle nested blocks.
  CurBlock = CurBlock->PrevBlockInfo;

  // FIXME: Delete the ParmVarDecl objects as well???

}

/// ActOnBlockStmtExpr - This is called when the body of a block statement
/// literal was successfully completed.  ^(int x){...}
Sema::ExprResult Sema::ActOnBlockStmtExpr(SourceLocation CaretLoc, StmtTy *body,
                                          Scope *CurScope) {
  // Ensure that CurBlock is deleted.
  llvm::OwningPtr<BlockSemaInfo> BSI(CurBlock);
  ExprOwningPtr<CompoundStmt> Body(this, static_cast<CompoundStmt*>(body));

  PopDeclContext();

  // Before poping CurBlock, set ActiveScope to this scopes parent.
  ActiveScope = CurBlock->TheScope->getParent();
  
  // Pop off CurBlock, handle nested blocks.
  CurBlock = CurBlock->PrevBlockInfo;

  QualType RetTy = Context.VoidTy;
  if (BSI->ReturnType)
    RetTy = QualType(BSI->ReturnType, 0);

  llvm::SmallVector<QualType, 8> ArgTypes;
  for (unsigned i = 0, e = BSI->Params.size(); i != e; ++i)
    ArgTypes.push_back(BSI->Params[i]->getType());

  QualType BlockTy;
  if (!BSI->hasPrototype)
    BlockTy = Context.getFunctionNoProtoType(RetTy);
  else
    BlockTy = Context.getFunctionType(RetTy, &ArgTypes[0], ArgTypes.size(),
                                      BSI->isVariadic, 0);

  BlockTy = Context.getBlockPointerType(BlockTy);

  BSI->TheDecl->setBody(Body.take());
  return new (Context) BlockExpr(BSI->TheDecl, BlockTy, BSI->hasBlockDeclRefExprs);
}

Sema::ExprResult Sema::ActOnVAArg(SourceLocation BuiltinLoc,
                                  ExprTy *expr, TypeTy *type,
                                  SourceLocation RPLoc) {
  Expr *E = static_cast<Expr*>(expr);
  QualType T = QualType::getFromOpaquePtr(type);

  InitBuiltinVaListType();

  // Get the va_list type
  QualType VaListType = Context.getBuiltinVaListType();
  // Deal with implicit array decay; for example, on x86-64,
  // va_list is an array, but it's supposed to decay to
  // a pointer for va_arg.
  if (VaListType->isArrayType())
    VaListType = Context.getArrayDecayedType(VaListType);
  // Make sure the input expression also decays appropriately.
  UsualUnaryConversions(E);

  if (CheckAssignmentConstraints(VaListType, E->getType()) != Compatible)
    return Diag(E->getLocStart(),
                diag::err_first_argument_to_va_arg_not_of_type_va_list)
      << E->getType() << E->getSourceRange();

  // FIXME: Warn if a non-POD type is passed in.

  return new (Context) VAArgExpr(BuiltinLoc, E, T.getNonReferenceType(), RPLoc);
}

Sema::ExprResult Sema::ActOnGNUNullExpr(SourceLocation TokenLoc) {
  // The type of __null will be int or long, depending on the size of
  // pointers on the target.
  QualType Ty;
  if (Context.Target.getPointerWidth(0) == Context.Target.getIntWidth())
    Ty = Context.IntTy;
  else
    Ty = Context.LongTy;

  return new (Context) GNUNullExpr(Ty, TokenLoc);
}

bool Sema::DiagnoseAssignmentResult(AssignConvertType ConvTy,
                                    SourceLocation Loc,
                                    QualType DstType, QualType SrcType,
                                    Expr *SrcExpr, const char *Flavor) {
  // Decode the result (notice that AST's are still created for extensions).
  bool isInvalid = false;
  unsigned DiagKind;
  switch (ConvTy) {
  default: assert(0 && "Unknown conversion type");
  case Compatible: return false;
  case PointerToInt:
    DiagKind = diag::ext_typecheck_convert_pointer_int;
    break;
  case IntToPointer:
    DiagKind = diag::ext_typecheck_convert_int_pointer;
    break;
  case IncompatiblePointer:
    DiagKind = diag::ext_typecheck_convert_incompatible_pointer;
    break;
  case FunctionVoidPointer:
    DiagKind = diag::ext_typecheck_convert_pointer_void_func;
    break;
  case CompatiblePointerDiscardsQualifiers:
    // If the qualifiers lost were because we were applying the
    // (deprecated) C++ conversion from a string literal to a char*
    // (or wchar_t*), then there was no error (C++ 4.2p2).  FIXME:
    // Ideally, this check would be performed in
    // CheckPointerTypesForAssignment. However, that would require a
    // bit of refactoring (so that the second argument is an
    // expression, rather than a type), which should be done as part
    // of a larger effort to fix CheckPointerTypesForAssignment for
    // C++ semantics.
    if (getLangOptions().CPlusPlus &&
        IsStringLiteralToNonConstPointerConversion(SrcExpr, DstType))
      return false;
    DiagKind = diag::ext_typecheck_convert_discards_qualifiers;
    break;
  case IntToBlockPointer:
    DiagKind = diag::err_int_to_block_pointer;
    break;
  case IncompatibleBlockPointer:
    DiagKind = diag::ext_typecheck_convert_incompatible_block_pointer;
    break;
  case IncompatibleObjCQualifiedId:
    // FIXME: Diagnose the problem in ObjCQualifiedIdTypesAreCompatible, since
    // it can give a more specific diagnostic.
    DiagKind = diag::warn_incompatible_qualified_id;
    break;
  case IncompatibleVectors:
    DiagKind = diag::warn_incompatible_vectors;
    break;
  case Incompatible:
    DiagKind = diag::err_typecheck_convert_incompatible;
    isInvalid = true;
    break;
  }

  Diag(Loc, DiagKind) << DstType << SrcType << Flavor
    << SrcExpr->getSourceRange();
  return isInvalid;
}

bool Sema::VerifyIntegerConstantExpression(const Expr* E, llvm::APSInt *Result)
{
  Expr::EvalResult EvalResult;

  if (!E->Evaluate(EvalResult, Context) || !EvalResult.Val.isInt() ||
      EvalResult.HasSideEffects) {
    Diag(E->getExprLoc(), diag::err_expr_not_ice) << E->getSourceRange();

    if (EvalResult.Diag) {
      // We only show the note if it's not the usual "invalid subexpression"
      // or if it's actually in a subexpression.
      if (EvalResult.Diag != diag::note_invalid_subexpr_in_ice ||
          E->IgnoreParens() != EvalResult.DiagExpr->IgnoreParens())
        Diag(EvalResult.DiagLoc, EvalResult.Diag);
    }

    return true;
  }

  if (EvalResult.Diag) {
    Diag(E->getExprLoc(), diag::ext_expr_not_ice) <<
      E->getSourceRange();

    // Print the reason it's not a constant.
    if (Diags.getDiagnosticLevel(diag::ext_expr_not_ice) != Diagnostic::Ignored)
      Diag(EvalResult.DiagLoc, EvalResult.Diag);
  }

  if (Result)
    *Result = EvalResult.Val.getInt();
  return false;
}
