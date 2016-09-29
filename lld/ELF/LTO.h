//===- LTO.h ----------------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides a way to combine bitcode files into one ELF
// file by compiling them using LLVM.
//
// If LTO is in use, your input files are not in regular ELF files
// but instead LLVM bitcode files. In that case, the linker has to
// convert bitcode files into the native format so that we can create
// an ELF file that contains native code. This file provides that
// functionality.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_ELF_LTO_H
#define LLD_ELF_LTO_H

#include "lld/Core/LLVM.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/IR/Module.h"
#include "llvm/Linker/IRMover.h"

namespace llvm {
namespace lto {
class LTO;
}
}

namespace lld {
namespace elf {

class BitcodeFile;
class InputFile;

class BitcodeCompiler {
public:
  BitcodeCompiler();
  ~BitcodeCompiler();
  void add(BitcodeFile &F);
  std::vector<InputFile *> compile();

  std::unique_ptr<llvm::lto::LTO> LtoObj;

private:
  std::vector<SmallString<0>> Buff;
};
}
}

#endif
