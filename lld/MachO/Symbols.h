#ifndef macho_syms
#define macho_syms

#include "../ELF/Strings.h"

namespace lld {
namespace mach_o2 {

struct InputSection;

class Symbol {
 public:
  enum Kind {
    DefinedKind,
    UndefinedKind,
  };

  Kind kind() const { return static_cast<Kind>(SymbolKind); }

  StringRef getName() const { return Name; }

protected:
  Symbol(Kind K, lld::elf::StringRefZ Name) : SymbolKind(K), Name(Name) {}
  Kind SymbolKind;
  lld::elf::StringRefZ Name;
};

class Defined : public Symbol {
public:
  Defined(lld::elf::StringRefZ Name, InputSection *IS, uint32_t Value)
      : Symbol(DefinedKind, Name), IS(IS), Value(Value) {}

  InputSection *IS;
  uint32_t Value;

  static bool classof(const Symbol *S) { return S->kind() == DefinedKind; }
};

class Undefined : public Symbol {
public:
  Undefined(lld::elf::StringRefZ Name) : Symbol(UndefinedKind, Name) {}

  static bool classof(const Symbol *S) { return S->kind() == UndefinedKind; }
};

union SymbolUnion {
  alignas(Defined) char A[sizeof(Defined)];
  alignas(Undefined) char B[sizeof(Undefined)];
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

}
}

#endif
