#include "InputFiles.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Support/MemoryBuffer.h"
#include "lld/Common/Memory.h"
#include "InputSection.h"
#include "OutputSegment.h"
#include "SymbolTable.h"
#include "Symbols.h"

#include <map>

using namespace lld;
using namespace mach_o2;
using namespace llvm::MachO;

InputFile *mach_o2::createObjectFile(MemoryBufferRef MBRef) {
  assert(MBRef.getBufferSize() >= sizeof(mach_header_64));

  auto *MH = reinterpret_cast<const mach_header_64 *>(MBRef.getBufferStart());
  assert(MH->magic == MH_MAGIC_64);

  ArrayRef<section_64> Sections;
  ArrayRef<nlist_64> Symbols;
  const char *Strtab = nullptr;

  size_t NCmds = MH->ncmds;
  auto *Cmds = reinterpret_cast<const char *>(MH + 1);
  while (NCmds--) {
    auto *Cmd = reinterpret_cast<const load_command *>(Cmds);
    Cmds += Cmd->cmdsize;

    switch (Cmd->cmd) {
    case LC_SEGMENT_64: {
      auto *SegmentCmd = reinterpret_cast<const segment_command_64 *>(Cmd);
      Sections = {reinterpret_cast<const section_64 *>(SegmentCmd + 1),
                  SegmentCmd->nsects};
      break;
    }
    case LC_SYMTAB: {
      auto *SymtabCmd = reinterpret_cast<const symtab_command *>(Cmd);
      Symbols = {reinterpret_cast<const nlist_64 *>(MBRef.getBufferStart() +
                                                    SymtabCmd->symoff),
                 SymtabCmd->nsyms};
      Strtab = MBRef.getBufferStart() + SymtabCmd->stroff;
      break;
    }
    }
  }

  InputFile *F = make<InputFile>();

  bool SubsectionsViaSymbols = MH->flags & MH_SUBSECTIONS_VIA_SYMBOLS;
  std::vector<std::map<uint32_t, InputSection *>> Subsections(Sections.size());
  std::vector<uint32_t> AltEntrySyms;

  for (unsigned I = 0; I != Sections.size(); ++I) {
    auto *IS = make<InputSection>();
    IS->Data = {reinterpret_cast<const uint8_t *>(MBRef.getBufferStart() +
                                                  Sections[I].offset),
                Sections[I].size};
    IS->Align = Sections[I].align;
    Subsections[I][0] = IS;
  }

  std::vector<Symbol *> Syms(Symbols.size());

  auto CreateDefined = [&](const nlist_64 &Sym, InputSection *IS,
                           uint32_t Value) -> Symbol * {
    if (Sym.n_type & N_EXT)
      return Symtab->addDefined(Strtab + Sym.n_strx, IS, Value);
    else
      return make<Defined>(Strtab + Sym.n_strx, IS, Value);
  };

  for (unsigned I = 0; I != Symbols.size(); ++I) {
    auto &Sym = Symbols[I];
    if (!Sym.n_sect) {
      Syms[I] = Symtab->addUndefined(Strtab + Sym.n_strx); // undef
      continue;
    }

    uint64_t Value = Sym.n_value - Sections[Sym.n_sect - 1].addr;

    // If the input file does not use subsections-via-symbols, all symbols can
    // use the same subsection.
    if (!SubsectionsViaSymbols) {
      Syms[I] = CreateDefined(Sym, Subsections[Sym.n_sect - 1][0], Value);
      continue;
    }

    // We can't create alt-entry symbols at this point because a later symbol
    // may split its section, which may affect which subsection the alt-entry
    // symbol is assigned to. So we need to handle them in a second pass below.
    if (Sym.n_desc & N_ALT_ENTRY) {
      AltEntrySyms.push_back(I);
      continue;
    }

    // Find the subsection corresponding to the greatest section offset that is
    // <= that of the current symbol. The subsection that we find either needs
    // to be used directly or split in two.
    auto &Subsec = Subsections[Sym.n_sect - 1];
    auto It = Subsec.upper_bound(Value);
    --It;
    assert(It != Subsec.end());

    if (It->first == Value) {
      // Alias of an existing symbol, or the first symbol in the section. These
      // are handled by reusing the existing section.
      Syms[I] = CreateDefined(Sym, It->second, 0);
      continue;
    }

    // We saw a symbol definition at a new offset. Split the section into two
    // subsections. The existing symbols use the first subsection and this
    // symbol uses the second one.
    auto *FirstIS = It->second;
    size_t FirstSize = Value - It->first;
    auto *SecondIS = make<InputSection>();
    Subsec[Value] = SecondIS;

    SecondIS->Data = {FirstIS->Data.data() + FirstSize,
                      FirstIS->Data.size() - FirstSize};
    SecondIS->Align = std::min(Sections[Sym.n_sect - 1].align,
                               1u << llvm::countTrailingZeros(Value));

    FirstIS->Data = {FirstIS->Data.data(), FirstSize};

    Syms[I] = CreateDefined(Sym, SecondIS, 0);
  }

  for (unsigned I : AltEntrySyms) {
    auto &Sym = Symbols[I];
    auto &Subsec = Subsections[Sym.n_sect - 1];
    uint64_t Value = Sym.n_value - Sections[Sym.n_sect - 1].addr;
    auto It = Subsec.upper_bound(Value);
    --It;
    assert(It != Subsec.end());
    Syms[I] = CreateDefined(Sym, It->second, Value - It->first);
  }

  for (unsigned I = 0; I != Sections.size(); ++I) {
    OutputSegment *&OS = OutputSegments[StringRef(
        Sections[I].segname, strnlen(Sections[I].segname, 16))];
    if (!OS)
      OS = make<OutputSegment>();
    auto &SectionVec = OS->Sections[StringRef(
        Sections[I].sectname, strnlen(Sections[I].sectname, 16))];
    SectionVec.reserve(Sections.size() + Subsections[I].size());
    for (auto &P : Subsections[I])
      SectionVec.push_back(P.second);
  }

  return F;
}
