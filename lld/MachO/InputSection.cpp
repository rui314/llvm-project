#include "InputSection.h"
#include "Symbols.h"
#include "llvm/Support/Endian.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm::MachO;
using namespace llvm::support;

void InputSection::writeTo(uint8_t *Buf) {
  memcpy(Buf, Data.data(), Data.size());

  for (auto &R : Relocs) {
    uint64_t VA;
    if (auto *S = R.Target.dyn_cast<Symbol *>())
      VA = S->getVA();
    else
      VA = R.Target.get<InputSection *>()->Addr;

    uint64_t B = Addr + R.Offset;
    switch (R.Type) {
      case X86_64_RELOC_BRANCH:
      case X86_64_RELOC_SIGNED: {
        *(ulittle32_t *)(Buf + R.Offset) = VA + R.Addend - B - 4;
        break;
      }
      default:
        assert(0);
    }
  }
}
