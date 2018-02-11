#include "InputSection.h"

using namespace lld;
using namespace lld::mach_o2;

void InputSection::writeTo(uint8_t *Buf) {
  memcpy(Buf, Data.data(), Data.size());
}
