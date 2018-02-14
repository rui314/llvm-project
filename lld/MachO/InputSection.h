#ifndef macho_inpusec
#define macho_inpusec

#include "llvm/BinaryFormat/MachO.h"
#include "llvm/ADT/ArrayRef.h"
#include "lld/Common/LLVM.h"

namespace lld {
namespace mach_o2 {

struct InputFile;

struct InputSection {
  InputFile *File;

  ArrayRef<uint8_t> Data;
  uint32_t Align;

  uint64_t Addr;

  uint32_t RelocOffset;
  SmallVector<ArrayRef<llvm::MachO::any_relocation_info>, 1> Relocs;

  void writeTo(uint8_t *Buf);
};

}
}

#endif

