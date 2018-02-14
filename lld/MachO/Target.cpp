#include "Target.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Support/Endian.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm::support;
using namespace llvm::MachO;

namespace {

struct X86_64 : TargetInfo {
  uint64_t getImplicitAddend(uint8_t *Loc, uint8_t Type) const;
  void relocateOne(uint8_t *Loc, uint8_t Type, uint64_t Val) const;
};

uint64_t X86_64::getImplicitAddend(uint8_t *Loc, uint8_t Type) const {
  switch (Type) {
    case X86_64_RELOC_BRANCH:
    case X86_64_RELOC_SIGNED:
    case X86_64_RELOC_SIGNED_1:
      return *(ulittle32_t *)Loc;
    default:
      assert(0);
  }
}

void X86_64::relocateOne(uint8_t *Loc, uint8_t Type, uint64_t Val) const {
  switch (Type) {
    case X86_64_RELOC_BRANCH:
    case X86_64_RELOC_SIGNED:
    case X86_64_RELOC_SIGNED_1: {
      *(ulittle32_t *)Loc = Val - 4;
      break;
    }
    default:
      assert(0);
  }
}

}

TargetInfo *mach_o2::createX86_64TargetInfo() {
  static X86_64 Targ;
  return &Targ;
}

TargetInfo *mach_o2::Target = nullptr;
