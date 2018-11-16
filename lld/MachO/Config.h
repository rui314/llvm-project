//===- Config.h -------------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_MACHO_CONFIG_H
#define LLD_MACHO_CONFIG_H

#include "lld/Common/LLVM.h"
#include "llvm/ADT/StringRef.h"

namespace lld {
namespace mach_o2 {

class Symbol;

struct Configuration {
  StringRef OutputFile;
  Symbol *Entry;
};

extern Configuration *Config;

} // namespace mach_o2
} // namespace lld

#endif
