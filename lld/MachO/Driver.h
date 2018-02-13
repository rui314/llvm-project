#pragma once

#include "llvm/Option/OptTable.h"
#include "lld/Common/LLVM.h"

namespace lld {
namespace mach_o2 {

class MachOOptTable : public llvm::opt::OptTable {
public:
  MachOOptTable();
  llvm::opt::InputArgList parse(ArrayRef<const char *> Argv);
};

// Create enum with OPT_xxx values for each option in Options.td
enum {
  OPT_INVALID = 0,
#define OPTION(_1, _2, ID, _4, _5, _6, _7, _8, _9, _10, _11, _12) OPT_##ID,
#include "Options.inc"
#undef OPTION
};

}
}
