#include "Target.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Support/Endian.h"

using namespace lld;
using namespace lld::macho;
using namespace llvm::support::endian;
using namespace llvm::MachO;

namespace {

struct X86_64 : TargetInfo {
  X86_64();
  uint64_t getImplicitAddend(const uint8_t *Loc, uint8_t Type) const;
  void relocateOne(uint8_t *Loc, uint8_t Type, uint64_t Val) const;
};

X86_64::X86_64() {
  CPUType = CPU_TYPE_X86_64;
  CPUSubtype = CPU_SUBTYPE_X86_64_ALL;
}

uint64_t X86_64::getImplicitAddend(const uint8_t *Loc, uint8_t Type) const {
  switch (Type) {
  case X86_64_RELOC_BRANCH:
  case X86_64_RELOC_SIGNED:
  case X86_64_RELOC_SIGNED_1:
    return read32le(Loc);
  default:
    assert(0);
  }
}

void X86_64::relocateOne(uint8_t *Loc, uint8_t Type, uint64_t Val) const {
  switch (Type) {
  case X86_64_RELOC_BRANCH:
  case X86_64_RELOC_SIGNED:
  case X86_64_RELOC_SIGNED_1:
    write32le(Loc, Val - 4);
    break;
  default:
    assert(0);
  }
}

} // namespace

TargetInfo *macho::createX86_64TargetInfo() {
  static X86_64 T;
  return &T;
}

TargetInfo *macho::Target = nullptr;
