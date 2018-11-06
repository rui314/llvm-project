#include "InputSection.h"
#include "Symbols.h"
#include "Target.h"
#include "llvm/Support/Endian.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm::MachO;
using namespace llvm::support;

void InputSection::writeTo(uint8_t *Buf) {
  memcpy(Buf, Data.data(), Data.size());

  for (Reloc &R : Relocs) {
    uint64_t VA;
    if (auto *S = R.Target.dyn_cast<Symbol *>())
      VA = S->getVA();
    else
      VA = R.Target.get<InputSection *>()->Addr;

    uint64_t Val = VA + R.Addend;
    if (R.HasImplicitAddend)
      Val += Target->getImplicitAddend(Buf + R.Offset, R.Type);
    if (1) // pcrel
      Val -= Addr + R.Offset;
    Target->relocateOne(Buf + R.Offset, R.Type, Val);
  }
}
