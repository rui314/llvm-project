#include "OutputSegment.h"
#include "lld/Common/Memory.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm;

std::vector<OutputSegment *> mach_o2::OutputSegments;

OutputSegment *mach_o2::getOrCreateOutputSegment(StringRef Name,
                                                 uint32_t Perms) {
  for (OutputSegment *OS : OutputSegments)
    if (OS->Name == Name)
      return OS;

  auto *OS = make<OutputSegment>();
  OS->Perms = Perms;
  OutputSegments.push_back(OS);
  return OS;
}
