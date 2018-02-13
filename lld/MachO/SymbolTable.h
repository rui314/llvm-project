#ifndef macho_symtab
#define macho_symtab

#include "llvm/ADT/CachedHashString.h"
#include "lld/Common/LLVM.h"

namespace lld {
namespace mach_o2 {

struct InputSection;
class Symbol;

class SymbolTable {
public:
  Symbol *addUndefined(StringRef Name);
  Symbol *addDefined(StringRef Name, InputSection *IS, uint32_t Value);

  Symbol *find(StringRef Name);

private:
  std::pair<Symbol *, bool> insert(StringRef Name);
  llvm::DenseMap<llvm::CachedHashStringRef, int> SymMap;
  std::vector<Symbol *> SymVector;
};

extern SymbolTable *Symtab;

}
}

#endif
