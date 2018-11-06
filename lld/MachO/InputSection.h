//===- InputSection.h -------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_MACHO_INPUT_SECTION_H
#define LLD_MACHO_INPUT_SECTION_H

#include "lld/Common/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/BinaryFormat/MachO.h"

namespace lld {
namespace mach_o2 {

class InputFile;
class Symbol;

class InputSection {
public:
  void writeTo(uint8_t *Buf);
  InputSection *splitAt(uint32_t Offset);

  InputFile *File;

  ArrayRef<uint8_t> Data;
  uint32_t Align;

  uint64_t Addr;

  struct Reloc {
    uint8_t Type;
    bool HasImplicitAddend;
    uint32_t Addend;
    uint32_t Offset;
    llvm::PointerUnion<Symbol *, InputSection *> Target;
  };

  std::vector<Reloc> Relocs;
};

} // namespace mach_o2
} // namespace lld

#endif
