#ifndef macho_outseg
#define macho_outseg

#include "llvm/ADT/MapVector.h"
#include "lld/Common/LLVM.h"

namespace lld {
namespace mach_o2 {

struct InputSection;

struct OutputSegment {
  uint32_t Perms;
  llvm::MapVector<StringRef, std::vector<InputSection *>> Sections;
};

extern llvm::MapVector<StringRef, OutputSegment *> OutputSegments;

}
}

#endif
