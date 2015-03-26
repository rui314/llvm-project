//===--------- lib/ReaderWriter/ELF/ARM/ARMLinkingContext.h ---------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_READER_WRITER_ELF_ARM_ARM_LINKING_CONTEXT_H
#define LLD_READER_WRITER_ELF_ARM_ARM_LINKING_CONTEXT_H

#include "lld/ReaderWriter/ELFLinkingContext.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/ELF.h"

namespace lld {
namespace elf {

class ARMLinkingContext final : public ELFLinkingContext {
public:
  static std::unique_ptr<ELFLinkingContext> create(llvm::Triple);
  ARMLinkingContext(llvm::Triple);

  bool isRelaOutputFormat() const override { return false; }

  void addPasses(PassManager &) override;

  uint64_t getBaseAddress() const override {
    if (_baseAddress == 0)
      return 0x400000;
    return _baseAddress;
  }

  bool isPLTRelocation(const Reference &r) const override {
    if (r.kindNamespace() != Reference::KindNamespace::ELF)
      return false;
    assert(r.kindArch() == Reference::KindArch::ARM);
    switch (r.kindValue()) {
    case llvm::ELF::R_ARM_JUMP_SLOT:
    case llvm::ELF::R_ARM_IRELATIVE:
      return true;
    default:
      return false;
    }
  }
};

// Special methods to check code model of atoms.
bool isARMCode(const DefinedAtom *atom);
bool isARMCode(DefinedAtom::CodeModel codeModel);
bool isThumbCode(const DefinedAtom *atom);
bool isThumbCode(DefinedAtom::CodeModel codeModel);

} // end namespace elf
} // end namespace lld

#endif
