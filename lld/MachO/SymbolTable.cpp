#include "SymbolTable.h"
#include "Symbols.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"

using namespace lld;
using namespace mach_o2;

std::pair<Symbol *, bool> SymbolTable::insert(StringRef Name) {
  auto P =
      SymMap.insert({llvm::CachedHashStringRef(Name), (int)SymVector.size()});
  Symbol *Sym;
  if (P.second) {
    Sym = (Symbol *)make<SymbolUnion>();
    SymVector.push_back(Sym);
  } else
    Sym = SymVector[P.first->second];

  return {Sym, P.second};
}

Symbol *SymbolTable::addUndefined(StringRef Name) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(Name);
  if (WasInserted)
    replaceSymbol<Undefined>(S, Name);
  return S;
}

Symbol *SymbolTable::addDefined(StringRef Name, InputSection *IS, uint32_t Value) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(Name);
  if (!WasInserted && isa<Defined>(S))
    error("duplicate symbol: " + Name);
  replaceSymbol<Defined>(S, Name, IS, Value);
  return S;
}

SymbolTable *mach_o2::Symtab = nullptr;
