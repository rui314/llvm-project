#ifndef macho_inp
#define macho_inp

#include <vector>
#include "lld/Common/LLVM.h"
#include "llvm/Support/MemoryBuffer.h"

namespace lld {
namespace mach_o2 {

struct InputSection;
class Symbol;

struct InputFile {
  InputFile(MemoryBufferRef MB) : MB(MB) {}
  MemoryBufferRef MB;
  std::vector<Symbol *> Syms;

  void parse();
};

InputFile *createObjectFile(MemoryBufferRef MBRef);

}
}

#endif

