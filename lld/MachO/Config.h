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

#include <vector>

namespace lld {
namespace macho {

class Symbol;

struct Configuration {
  StringRef OutputFile;
  Symbol *Entry;

  std::vector<llvm::StringRef> SearchPaths;
};

extern Configuration *Config;

} // namespace macho
} // namespace lld

#endif
