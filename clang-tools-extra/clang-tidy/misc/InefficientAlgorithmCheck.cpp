//===--- InefficientAlgorithmCheck.cpp - clang-tidy------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "InefficientAlgorithmCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace misc {

static bool areTypesCompatible(QualType Left, QualType Right) {
  if (const auto *LeftRefType = Left->getAs<ReferenceType>())
    Left = LeftRefType->getPointeeType();
  if (const auto *RightRefType = Right->getAs<ReferenceType>())
    Right = RightRefType->getPointeeType();
  return Left->getCanonicalTypeUnqualified() ==
         Right->getCanonicalTypeUnqualified();
}

void InefficientAlgorithmCheck::registerMatchers(MatchFinder *Finder) {
  const std::string Algorithms =
      "^::std::(find|count|equal_range|lower_bound|upper_bound)$";
  const auto ContainerMatcher = classTemplateSpecializationDecl(
      matchesName("^::std::(unordered_)?(multi)?(set|map)$"));
  const auto Matcher =
      callExpr(
          callee(functionDecl(matchesName(Algorithms))),
          hasArgument(
              0, constructExpr(has(memberCallExpr(
                     callee(methodDecl(hasName("begin"))),
                     on(declRefExpr(
                            hasDeclaration(decl().bind("IneffContObj")),
                            anyOf(hasType(ContainerMatcher.bind("IneffCont")),
                                  hasType(pointsTo(
                                      ContainerMatcher.bind("IneffContPtr")))))
                            .bind("IneffContExpr")))))),
          hasArgument(1, constructExpr(has(memberCallExpr(
                             callee(methodDecl(hasName("end"))),
                             on(declRefExpr(hasDeclaration(
                                 equalsBoundNode("IneffContObj")))))))),
          hasArgument(2, expr().bind("AlgParam")),
          unless(isInTemplateInstantiation())).bind("IneffAlg");

  Finder->addMatcher(Matcher, this);
}

void InefficientAlgorithmCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *AlgCall = Result.Nodes.getNodeAs<CallExpr>("IneffAlg");
  const auto *IneffCont =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("IneffCont");
  bool PtrToContainer = false;
  if (!IneffCont) {
    IneffCont =
        Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("IneffContPtr");
    PtrToContainer = true;
  }
  const llvm::StringRef IneffContName = IneffCont->getName();
  const bool Unordered =
      IneffContName.find("unordered") != llvm::StringRef::npos;
  const bool Maplike = IneffContName.find("map") != llvm::StringRef::npos;

  // Store if the key type of the container is compatible with the value
  // that is searched for.
  QualType ValueType = AlgCall->getArg(2)->getType();
  QualType KeyType =
      IneffCont->getTemplateArgs()[0].getAsType().getCanonicalType();
  const bool CompatibleTypes = areTypesCompatible(KeyType, ValueType);

  // Check if the comparison type for the algorithm and the container matches.
  if (AlgCall->getNumArgs() == 4 && !Unordered) {
    const Expr *Arg = AlgCall->getArg(3);
    const QualType AlgCmp =
        Arg->getType().getUnqualifiedType().getCanonicalType();
    const unsigned CmpPosition =
        (IneffContName.find("map") == llvm::StringRef::npos) ? 1 : 2;
    const QualType ContainerCmp = IneffCont->getTemplateArgs()[CmpPosition]
                                      .getAsType()
                                      .getUnqualifiedType()
                                      .getCanonicalType();
    if (AlgCmp != ContainerCmp) {
      diag(Arg->getLocStart(),
           "different comparers used in the algorithm and the container");
      return;
    }
  }

  const auto *AlgDecl = AlgCall->getDirectCallee();
  if (!AlgDecl)
    return;

  if (Unordered && AlgDecl->getName().find("bound") != llvm::StringRef::npos)
    return;

  const auto *AlgParam = Result.Nodes.getNodeAs<Expr>("AlgParam");
  const auto *IneffContExpr = Result.Nodes.getNodeAs<Expr>("IneffContExpr");
  FixItHint Hint;

  SourceManager &SM = *Result.SourceManager;
  LangOptions LangOpts = Result.Context->getLangOpts();

  CharSourceRange CallRange =
      CharSourceRange::getTokenRange(AlgCall->getSourceRange());

  // FIXME: Create a common utility to extract a file range that the given token
  // sequence is exactly spelled at (without macro argument expansions etc.).
  // We can't use Lexer::makeFileCharRange here, because for
  //
  //   #define F(x) x
  //   x(a b c);
  //
  // it will return "x(a b c)", when given the range "a"-"c". It makes sense for
  // removals, but not for replacements.
  //
  // This code is over-simplified, but works for many real cases.
  if (SM.isMacroArgExpansion(CallRange.getBegin()) &&
      SM.isMacroArgExpansion(CallRange.getEnd())) {
    CallRange.setBegin(SM.getSpellingLoc(CallRange.getBegin()));
    CallRange.setEnd(SM.getSpellingLoc(CallRange.getEnd()));
  }

  if (!CallRange.getBegin().isMacroID() && !Maplike && CompatibleTypes) {
    StringRef ContainerText = Lexer::getSourceText(
        CharSourceRange::getTokenRange(IneffContExpr->getSourceRange()), SM,
        LangOpts);
    StringRef ParamText = Lexer::getSourceText(
        CharSourceRange::getTokenRange(AlgParam->getSourceRange()), SM,
        LangOpts);
    std::string ReplacementText =
        (llvm::Twine(ContainerText) + (PtrToContainer ? "->" : ".") +
         AlgDecl->getName() + "(" + ParamText + ")")
            .str();
    Hint = FixItHint::CreateReplacement(CallRange, ReplacementText);
  }

  diag(AlgCall->getLocStart(),
       "this STL algorithm call should be replaced with a container method")
      << Hint;
}

} // namespace misc
} // namespace tidy
} // namespace clang
