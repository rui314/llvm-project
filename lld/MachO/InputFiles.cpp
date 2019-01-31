//===- InputFiles.cpp -------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains functions to parse Mach-O object files. In this comment,
// we describe the Mach-O file structure and how we parse it.
//
// Mach-O is not very different from ELF or COFF. The notion of symbols,
// sections and relocations exsits in Mach-O as they do in ELF and COFF.
//
// Perhaps the notion that is new to those who know ELF/COFF is "subsections".
// In ELF/COFF, sections are an atomic unit of data copied from input files to
// output files. When we merge or garbage-collect sections, we treat each
// section as an atomic unit. In Mach-O, that's not the case. Sections can
// consist of multiple subsections, and subsections are a unit of merging and
// garbage-collecting. Therefore, Mach-O's subsections are more similar to
// ELF/COFF's sections than Mach-O's sections are.
//
// A section can have multiple symbols. A symbol that does not have
// N_ALT_ENTRY attribute indicates a beginning of a subsection. Therefore, by
// default, a symbol is always present at beginning of each subsection. A
// symbol with N_ALT_ENTRY attribute does not start a new subsection and can
// point to a middle of a subsection. In this file, we split sections into
// multiple subsections by scanning a symbol table.
//
// The notion of subsections also affects how relocations are represented in
// Mach-O. All references within a section need to be explicitly represented
// as relocations if they refer different subsections, because we obviously
// need to fix up addresses if subsections are laid out in an output file
// differently than they were in object files. To represent that, Mach-O
// relocation can refer an unnamed location of the same section. Therefore,
// Mach-O relocation has a bit indicating whether it refers a symbol or a
// location within the same section. R_SCATTERED is that bit.
//
// Without the above differences, I think you can use your knowledge about ELF
// and COFF for Mach-O.
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "InputSection.h"
#include "OutputSegment.h"
#include "SymbolTable.h"
#include "Symbols.h"
#include "Target.h"

#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Support/MemoryBuffer.h"

#include <map>

using namespace lld;
using namespace llvm;
using namespace mach_o2;
using namespace llvm::MachO;

std::vector<InputFile *> mach_o2::InputFiles;

namespace {
struct MachOFile {
  const mach_header_64 *Header;
  ArrayRef<section_64> Sections;
  ArrayRef<nlist_64> Symbols;
  const char *Strtab = nullptr;
};
} // namespace

static MachOFile parseFile(MemoryBufferRef MB) {
  MachOFile File;
  assert(MB.getBufferSize() >= sizeof(mach_header_64));

  const uint8_t *Buf = reinterpret_cast<const uint8_t *>(MB.getBufferStart());
  const uint8_t *P = Buf;

  File.Header = reinterpret_cast<const mach_header_64 *>(P);
  assert(File.Header->magic == MH_MAGIC_64);

  P += sizeof(mach_header_64);

  for (size_t I = 0; I < File.Header->ncmds; ++I) {
    auto *Cmd = reinterpret_cast<const load_command *>(P);

    if (Cmd->cmd == LC_SEGMENT_64) {
      auto *Seg = reinterpret_cast<const segment_command_64 *>(Cmd);
      File.Sections = {reinterpret_cast<const section_64 *>(Seg + 1),
                       Seg->nsects};
    } else if (Cmd->cmd == LC_SYMTAB) {
      auto *Syms = reinterpret_cast<const symtab_command *>(Cmd);
      File.Symbols = {reinterpret_cast<const nlist_64 *>(Buf + Syms->symoff),
                      Syms->nsyms};
      File.Strtab = (const char *)Buf + Syms->stroff;
    }
    P += Cmd->cmdsize;
  }

  return File;
}

ObjFile::ObjFile(MemoryBufferRef MB) : InputFile(ObjKind, MB) {
  MachOFile File = parseFile(MB);

  Sections.reserve(File.Sections.size());
  Symbols.reserve(File.Symbols.size());

  auto *Buf = reinterpret_cast<const uint8_t *>(MB.getBufferStart());

  // Initialize Sections.
  for (const section_64 &Sec : File.Sections) {
    InputSection *IS = make<InputSection>();
    IS->File = this;
    IS->Name = StringRef(Sec.segname, strnlen(Sec.segname, 16));
    IS->Data = {Buf + Sec.offset, Sec.size};
    IS->Align = Sec.align;
    Sections.push_back(IS);
  }

  // Initialize Symbols.
  for (const nlist_64 &Sym : File.Symbols) {
    StringRef Name = File.Strtab + Sym.n_strx;

    // Undefined symbol
    if (!Sym.n_sect) {
      Symbols.push_back(Symtab->addUndefined(Name));
      continue;
    }

    uint64_t Value = Sym.n_value - Sections[Sym.n_sect - 1]->Addr;
    InputSection *IS = Sections[Sym.n_sect - 1];

    // Global defined symbol
    if (Sym.n_type & N_EXT) {
      Symbols.push_back(Symtab->addDefined(Name, IS, Value));
      continue;
    }

    // Local defined symbol
    Symbols.push_back(make<Defined>(Name, IS, Value));
  }

  // Initialize relocation vectors.
  for (size_t I = 0; I != File.Sections.size(); ++I) {
    const section_64 &Sec = File.Sections[I];

    // Assign relocations to subsections.
    ArrayRef<any_relocation_info> Relocs{
      reinterpret_cast<const any_relocation_info *>(Buf + Sec.reloff),
        Sec.nreloc};

    for (const any_relocation_info &Rel : Relocs) {
      InputSection::Reloc R;
      R.Offset = Rel.r_word0 & 0xffffff;

      if (Rel.r_word0 & R_SCATTERED) {
        R.Type = (Rel.r_word0 >> 24) & 0xf;
	R.HasImplicitAddend = true;
	R.Addend = Rel.r_word1;
      } else {
        R.Type = Rel.r_word1 >> 28;
	R.HasImplicitAddend = false;
	R.Addend = Target->getImplicitAddend(Buf + Sec.offset + R.Offset, R.Type);
      }

      Sections[I]->Relocs.push_back(R);
    }
  }
}

DylibFile::DylibFile(MemoryBufferRef MB) : InputFile(DylibKind, MB) {
  MachOFile File = parseFile(MB);
  Symbols.reserve(File.Symbols.size());
}

ArchiveFile::ArchiveFile(std::unique_ptr<llvm::object::Archive> &F)
  : InputFile(ArchiveKind, F->getMemoryBufferRef()),
    File(std::move(F)) {
  for (const object::Archive::Symbol &Sym : File->symbols())
    Symtab->addLazy(Sym.getName(), *this, Sym);
}

InputFile *ArchiveFile::fetch(const object::Archive::Symbol &Sym) {
  object::Archive::Child C =
      CHECK(Sym.getMember(), toString(this) +
                                 ": could not get the member for symbol " +
                                 Sym.getName());

  if (!Seen.insert(C.getChildOffset()).second)
    return nullptr;

  MemoryBufferRef MB =
      CHECK(C.getMemoryBufferRef(),
            toString(this) +
                ": could not get the buffer for the member defining symbol " +
                Sym.getName());
  return make<ObjFile>(MB);
}

// Returns "<internal>", "foo.a(bar.o)" or "baz.o".
std::string lld::toString(const InputFile *File) {
  return File ? File->getName() : "<internal>";
}
