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

#include "lld/Common/LLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include <vector>

namespace lld {
namespace mach_o2 {

class InputSection;
class Symbol;

class InputFile {
public:
  InputFile(MemoryBufferRef MB) : MB(MB) {}
  void parse();

  MemoryBufferRef MB;
  std::vector<Symbol *> Symbols;
  std::vector<InputSection *> Sections;
};

InputFile *createObjectFile(MemoryBufferRef MBRef);

} // namespace mach_o2
} // namespace lld

#endif
