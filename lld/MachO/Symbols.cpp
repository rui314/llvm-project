#include "Symbols.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Strings.h"

using namespace llvm;

using namespace lld;
using namespace lld::mach_o2;

InputFile *LazySymbol::fetch() {
  return nullptr;
}

// Returns a symbol for an error message.
std::string lld::toString(const Symbol &Sym) {
  if (Optional<std::string> S = demangleItanium(Sym.getName()))
    return *S;
  return Sym.getName();
}
