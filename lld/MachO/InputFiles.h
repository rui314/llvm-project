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
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Object/Archive.h"
#include "llvm/Support/MemoryBuffer.h"
#include <vector>

namespace lld {
namespace macho {

class InputSection;
class Symbol;

class InputFile {
public:
  enum Kind {
    ObjKind,
    DylibKind,
    ArchiveKind,
  };

  virtual ~InputFile() {}

  Kind kind() const { return FileKind; }
  StringRef getName() const { return MB.getBufferIdentifier(); }

  MemoryBufferRef MB;
  std::vector<Symbol *> Symbols;
  std::vector<InputSection *> Sections;
  StringRef DylibName;

protected:
  InputFile(Kind Kind, MemoryBufferRef MB) : MB(MB), FileKind(Kind) {}

  std::vector<InputSection *> parseSections(ArrayRef<const llvm::MachO::section_64>);

private:
  const Kind FileKind;
};

// .o file
class ObjFile : public InputFile {
public:
  explicit ObjFile(MemoryBufferRef MB);
  static bool classof(const InputFile *F) { return F->kind() == ObjKind; }
};

// .dylib file
class DylibFile : public InputFile {
public:
  explicit DylibFile(MemoryBufferRef MB);
  static bool classof(const InputFile *F) { return F->kind() == DylibKind; }
};

// .a file
class ArchiveFile : public InputFile {
public:
  explicit ArchiveFile(std::unique_ptr<llvm::object::Archive> &File);
  static bool classof(const InputFile *F) { return F->kind() == ArchiveKind; }
  InputFile *fetch(const llvm::object::Archive::Symbol &Sym);

private:
  std::unique_ptr<llvm::object::Archive> File;
  llvm::DenseSet<uint64_t> Seen;
};

InputFile *createObjectFile(MemoryBufferRef MBRef);

extern std::vector<InputFile *> InputFiles;

} // namespace macho

std::string toString(const macho::InputFile *File);
} // namespace lld

#endif
