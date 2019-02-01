#include "InputSection.h"
#include "Symbols.h"
#include "Target.h"
#include "llvm/Support/Endian.h"
#include "lld/Common/Memory.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm::MachO;
using namespace llvm::support;

std::vector<InputSection *> mach_o2::InputSections;

void InputSection::writeTo(uint8_t *Buf) {
  memcpy(Buf, Data.data(), Data.size());

  for (Reloc &R : Relocs) {
    uint64_t VA = 0;
    if (auto *S = R.Target.dyn_cast<Symbol *>())
      VA = S->getVA();
    else if (auto *IS = R.Target.dyn_cast<InputSection *>())
      VA = IS->Addr;

    uint64_t Val = VA + R.Addend;
    if (R.HasImplicitAddend)
      Val += Target->getImplicitAddend(Buf + R.Offset, R.Type);
    if (1) // pcrel
      Val -= Addr + R.Offset;
    Target->relocateOne(Buf + R.Offset, R.Type, Val);
  }
}
