#include "Symbols.h"
#include "InputFiles.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Strings.h"

using namespace llvm;

using namespace lld;
using namespace lld::macho;

InputFile *LazySymbol::fetch() {
  return cast<ArchiveFile>(File)->fetch(Sym);
}

// Returns a symbol for an error message.
std::string lld::toString(const Symbol &Sym) {
  if (Optional<std::string> S = demangleItanium(Sym.getName()))
    return *S;
  return Sym.getName();
}
