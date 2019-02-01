#include "SymbolTable.h"
#include "InputFiles.h"
#include "Symbols.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"

using namespace llvm;
using namespace lld;
using namespace mach_o2;

Symbol *SymbolTable::find(StringRef Name) {
  auto It = SymMap.find(llvm::CachedHashStringRef(Name));
  if (It == SymMap.end())
    return nullptr;
  return SymVector[It->second];
}

std::pair<Symbol *, bool> SymbolTable::insert(StringRef Name) {
  auto P = SymMap.insert({CachedHashStringRef(Name), (int)SymVector.size()});

  // Name already present in the symbol table.
  if (!P.second)
    return {SymVector[P.first->second], false};

  // Name is a new symbol.
  Symbol *Sym = (Symbol *)make<SymbolUnion>();
  SymVector.push_back(Sym);
  return {Sym, true};
}

Symbol *SymbolTable::addDefined(StringRef Name, InputSection *IS,
                                uint32_t Value) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(Name);

  if (!WasInserted && isa<Defined>(S))
    error("duplicate symbol: " + Name);

  replaceSymbol<Defined>(S, Name, IS, Value);
  return S;
}

Symbol *SymbolTable::addUndefined(StringRef Name) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(Name);

  if (WasInserted)
    replaceSymbol<Undefined>(S, Name);
  return S;
}

Symbol *SymbolTable::addDylib(StringRef Name, InputFile *File) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(Name);

  if (WasInserted)
    replaceSymbol<DylibSymbol>(S, File, Name);
  return S;
}

Symbol *SymbolTable::addLazy(StringRef Name, ArchiveFile *File,
                             const llvm::object::Archive::Symbol Sym) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(Name);

  if (WasInserted)
    replaceSymbol<LazySymbol>(S, File, Sym);
  else if (isa<Undefined>(S))
    File->fetch(Sym);
  return S;
}

SymbolTable *mach_o2::Symtab;
