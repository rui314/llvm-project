#include "OutputSegment.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm;

MapVector<StringRef, OutputSegment *> mach_o2::OutputSegments;
