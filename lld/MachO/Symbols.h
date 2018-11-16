//===- Symbols.h ------------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_MACHO_SYMBOLS_H
#define LLD_MACHO_SYMBOLS_H

#include "InputSection.h"
#include "lld/Common/Strings.h"
#include "llvm/Object/Archive.h"

namespace lld {
namespace mach_o2 {

class InputSection;
class InputFile;
class ArchiveFile;

struct StringRefZ {
  StringRefZ(const char *S) : Data(S), Size(-1) {}
  StringRefZ(StringRef S) : Data(S.data()), Size(S.size()) {}

  const char *Data;
  const uint32_t Size;
};

class Symbol {
public:
  enum Kind {
    DefinedKind,
    UndefinedKind,
    LazyKind,
  };

  Kind kind() const { return static_cast<Kind>(SymbolKind); }

  StringRef getName() const { return {Name.Data, Name.Size}; }

  uint64_t getVA() const;

protected:
  Symbol(Kind K, StringRefZ Name) : SymbolKind(K), Name(Name) {}
  Kind SymbolKind;
  StringRefZ Name;
};

class Defined : public Symbol {
public:
  Defined(StringRefZ Name, InputSection *IS, uint32_t Value)
      : Symbol(DefinedKind, Name), IS(IS), Value(Value) {}

  InputSection *IS;
  uint32_t Value;

  static bool classof(const Symbol *S) { return S->kind() == DefinedKind; }
};

class Undefined : public Symbol {
public:
  Undefined(StringRefZ Name) : Symbol(UndefinedKind, Name) {}

  static bool classof(const Symbol *S) { return S->kind() == UndefinedKind; }
};

class LazySymbol : public Symbol {
public:
  LazySymbol(ArchiveFile &File, const llvm::object::Archive::Symbol Sym)
      : Symbol(LazyKind, Sym.getName()), File(File), Sym(Sym) {}

  static bool classof(const Symbol *S) { return S->kind() == LazyKind; }

  InputFile *fetch();

private:
  ArchiveFile &File;
  const llvm::object::Archive::Symbol Sym;
};

inline uint64_t Symbol::getVA() const {
  auto *D = cast<Defined>(this);
  return D->IS->Addr + D->Value;
}

union SymbolUnion {
  alignas(Defined) char A[sizeof(Defined)];
  alignas(Undefined) char B[sizeof(Undefined)];
  alignas(LazySymbol) char C[sizeof(LazySymbol)];
};

template <typename T, typename... ArgT>
void replaceSymbol(Symbol *S, ArgT &&... Arg) {
  static_assert(sizeof(T) <= sizeof(SymbolUnion), "SymbolUnion too small");
  static_assert(alignof(T) <= alignof(SymbolUnion),
                "SymbolUnion not aligned enough");
  assert(static_cast<Symbol *>(static_cast<T *>(nullptr)) == nullptr &&
         "Not a Symbol");

  new (S) T(std::forward<ArgT>(Arg)...);
}

} // namespace mach_o2
} // namespace lld

#endif
