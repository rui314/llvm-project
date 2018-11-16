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
#include "llvm/ADT/DenseSet.h"
#include "llvm/Object/Archive.h"
#include "llvm/Support/MemoryBuffer.h"
#include <vector>

namespace lld {
namespace mach_o2 {

class InputSection;
class Symbol;

class InputFile {
public:
  enum Kind {
    ObjKind,
    ArchiveKind,
  };

  virtual ~InputFile() {}

  Kind kind() const { return FileKind; }
  StringRef getName() const { return MB.getBufferIdentifier(); }
  virtual void parse() = 0;

  MemoryBufferRef MB;
  std::vector<Symbol *> Symbols;
  std::vector<InputSection *> Sections;

protected:
  InputFile(Kind Kind, MemoryBufferRef MB) : MB(MB), FileKind(Kind) {}

private:
  const Kind FileKind;
};

// .o file
class ObjFile : public InputFile {
public:
  explicit ObjFile(MemoryBufferRef MB) : InputFile(ObjKind, MB) {}

  static bool classof(const InputFile *F) { return F->kind() == ObjKind; }
  void parse() override;
};

// .a file
class ArchiveFile : public InputFile {
public:
  explicit ArchiveFile(std::unique_ptr<llvm::object::Archive> &&File)
      : InputFile(ArchiveKind, File->getMemoryBufferRef()),
        File(std::move(File)) {}

  static bool classof(const InputFile *F) { return F->kind() == ArchiveKind; }
  void parse() override;

  InputFile *fetch(const llvm::object::Archive::Symbol &Sym);

private:
  std::unique_ptr<llvm::object::Archive> File;
  llvm::DenseSet<uint64_t> Seen;
};

InputFile *createObjectFile(MemoryBufferRef MBRef);

extern std::vector<InputFile *> InputFiles;

} // namespace mach_o2

std::string toString(const mach_o2::InputFile *File);
} // namespace lld

#endif
