//===- InputFiles.h ---------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_MACHO_INPUT_FILES_H
#define LLD_MACHO_INPUT_FILES_H

#include <vector>
#include "lld/Common/LLVM.h"
#include "llvm/Support/MemoryBuffer.h"

namespace lld {
namespace mach_o2 {

struct InputSection;
class Symbol;

struct InputFile {
  InputFile(MemoryBufferRef MB) : MB(MB) {}
  MemoryBufferRef MB;
  std::vector<Symbol *> Syms;

  void parse();
};

InputFile *createObjectFile(MemoryBufferRef MBRef);

}
}

#endif
