#ifndef macho_inpusec
#define macho_inpusec

#include "llvm/BinaryFormat/MachO.h"

namespace lld {
namespace mach_o2 {

struct InputSection {
  ArrayRef<uint8_t> Data;

  uint32_t RelocOffset;
  SmallVector<ArrayRef<llvm::MachO::any_relocation_info>, 1> Relocs;
};

}
}

#endif

