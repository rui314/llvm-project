#ifndef macho_inpusec
#define macho_inpusec

#include "llvm/BinaryFormat/MachO.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/PointerUnion.h"
#include "lld/Common/LLVM.h"

namespace lld {
namespace mach_o2 {

struct InputFile;
class Symbol;

struct InputSection {
  InputFile *File;

  ArrayRef<uint8_t> Data;
  uint32_t Align;

  uint64_t Addr;

  struct Reloc {
    uint8_t Type;
    bool HasImplicitAddend;
    uint32_t Addend;
    uint32_t Offset;
    llvm::PointerUnion<Symbol *, InputSection *> Target;
  };
  std::vector<Reloc> Relocs;

  void writeTo(uint8_t *Buf);
};

}
}

#endif

