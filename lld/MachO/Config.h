#pragma once

#include "lld/Common/LLVM.h"
#include "llvm/ADT/StringRef.h"

namespace lld {
namespace mach_o2 {

class Symbol;

struct Configuration {
  Symbol *Entry;
  StringRef OutputFile;
};

extern Configuration *Config;

}
}
