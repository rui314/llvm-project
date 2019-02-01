//===- SymbolTable.h --------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_MACHO_SYMBOL_TABLE_H
#define LLD_MACHO_SYMBOL_TABLE_H

#include "lld/Common/LLVM.h"
#include "llvm/ADT/CachedHashString.h"
#include "llvm/Object/Archive.h"

namespace lld {
namespace mach_o2 {

class InputSection;
class ArchiveFile;
class Symbol;

class SymbolTable {
public:
  Symbol *addUndefined(StringRef Name);
  Symbol *addDefined(StringRef Name, InputSection *IS, uint32_t Value);
  Symbol *addLazy(StringRef Name, ArchiveFile &File,
                  const llvm::object::Archive::Symbol Sym);

  ArrayRef<Symbol *> getSymbols() const { return SymVector; }
  Symbol *find(StringRef Name);

private:
  std::pair<Symbol *, bool> insert(StringRef Name);
  llvm::DenseMap<llvm::CachedHashStringRef, int> SymMap;
  std::vector<Symbol *> SymVector;
};

extern SymbolTable *Symtab;

} // namespace mach_o2
} // namespace lld

#endif
