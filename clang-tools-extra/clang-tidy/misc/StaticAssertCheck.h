//===--- StaticAssertCheck.h - clang-tidy -----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_STATIC_ASSERT_CHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_STATIC_ASSERT_CHECK_H

#include "../ClangTidy.h"
#include "llvm/ADT/StringRef.h"
#include <string>

namespace clang {
namespace tidy {

/// \brief Replaces \c assert() with \c static_assert() if the condition is
/// evaluatable at compile time.
///
/// The condition of \c static_assert() is evaluated at compile time which is
/// safer and more efficient.
class StaticAssertCheck : public ClangTidyCheck {
public:
  StaticAssertCheck(StringRef Name, ClangTidyContext *Context);
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

private:
  SourceLocation getLastParenLoc(const ASTContext *ASTCtx,
                                 SourceLocation AssertLoc);
};

} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_STATIC_ASSERT_CHECK_H
