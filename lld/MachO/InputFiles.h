#ifndef macho_inp
#define macho_inp

#include <vector>
#include "lld/Common/LLVM.h"

namespace lld {
namespace mach_o2 {

struct InputSection;

struct InputFile {
  std::vector<InputSection *> Sections;
};

InputFile *createObjectFile(MemoryBufferRef MBRef);

}
}

#endif

