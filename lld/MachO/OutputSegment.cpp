#include "OutputSegment.h"
#include "lld/Common/Memory.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm;

MapVector<StringRef, OutputSegment *> mach_o2::OutputSegments;

OutputSegment *mach_o2::getOrCreateOutputSegment(StringRef Name, uint32_t Perms) {
  OutputSegment *&OS = OutputSegments[Name];
  if (!OS) {
    OS = make<OutputSegment>();
    OS->Perms = Perms;
  }
  return OS;
}
