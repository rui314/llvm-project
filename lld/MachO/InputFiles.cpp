#include "InputFiles.h"
#include "InputSection.h"
#include "OutputSegment.h"
#include "SymbolTable.h"
#include "Symbols.h"
#include "Target.h"
#include "lld/Common/Memory.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Support/MemoryBuffer.h"

#include <map>

using namespace lld;
using namespace mach_o2;
using namespace llvm::MachO;

InputFile *mach_o2::createObjectFile(MemoryBufferRef MBRef) {
  InputFile *F = make<InputFile>(MBRef);
  F->parse();
  return F;
}

static const load_command *findLoadCommand(const mach_header_64 *MH,
                                           uint32_t Ty, const uint8_t *Buf) {
  Buf += sizeof(mach_header_64);
  for (size_t I = 0; I < MH->ncmds; ++I) {
    auto *Cmd = reinterpret_cast<const load_command *>(Buf);
    if (Cmd->cmd == Ty)
      return Cmd;
    Buf += Cmd->cmdsize;
  }
  return nullptr;
}

void InputFile::parse() {
  assert(MB.getBufferSize() >= sizeof(mach_header_64));
  auto *Buf = reinterpret_cast<const uint8_t *>(MB.getBufferStart());

  auto *MH = reinterpret_cast<const mach_header_64 *>(Buf);
  assert(MH->magic == MH_MAGIC_64);

  ArrayRef<section_64> Sections;
  if (const load_command *Cmd = findLoadCommand(MH, LC_SEGMENT_64, Buf)) {
    auto *Seg = reinterpret_cast<const segment_command_64 *>(Cmd);
    Sections = {reinterpret_cast<const section_64 *>(Seg + 1), Seg->nsects};
  }

  ArrayRef<nlist_64> Symbols;
  const char *Strtab = nullptr;
  if (const load_command *Cmd = findLoadCommand(MH, LC_SYMTAB, Buf)) {
    auto *Syms = reinterpret_cast<const symtab_command *>(Cmd);
    Symbols = {reinterpret_cast<const nlist_64 *>(Buf + Syms->symoff),
               Syms->nsyms};
    Strtab = (const char *)Buf + Syms->stroff;
  }

  bool SubsectionsViaSymbols = MH->flags & MH_SUBSECTIONS_VIA_SYMBOLS;
  std::vector<std::map<uint32_t, InputSection *>> Subsections(Sections.size());
  std::vector<uint32_t> AltEntrySyms;

  for (unsigned I = 0; I != Sections.size(); ++I) {
    InputSection *IS = make<InputSection>();
    IS->File = this;
    IS->Data = {Buf + Sections[I].offset, Sections[I].size};
    IS->Align = Sections[I].align;
    Subsections[I][0] = IS;
  }

  Syms.resize(Symbols.size());

  auto CreateDefined = [&](const nlist_64 &Sym, InputSection *IS,
                           uint32_t Value) -> Symbol * {
    if (Sym.n_type & N_EXT)
      return Symtab->addDefined(Strtab + Sym.n_strx, IS, Value);
    return make<Defined>(Strtab + Sym.n_strx, IS, Value);
  };

  for (unsigned I = 0; I != Symbols.size(); ++I) {
    const nlist_64 &Sym = Symbols[I];
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
    std::map<uint32_t, InputSection *> &Subsec = Subsections[Sym.n_sect - 1];
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
    InputSection *FirstIS = It->second;
    InputSection *SecondIS = FirstIS->splitAt(Value - It->first);
    Subsec[Value] = SecondIS;
    Syms[I] = CreateDefined(Sym, SecondIS, 0);
  }

  for (unsigned I : AltEntrySyms) {
    const nlist_64 &Sym = Symbols[I];
    std::map<uint32_t, InputSection *> &Subsec = Subsections[Sym.n_sect - 1];
    uint64_t Value = Sym.n_value - Sections[Sym.n_sect - 1].addr;
    auto It = Subsec.upper_bound(Value);
    --It;
    assert(It != Subsec.end());
    Syms[I] = CreateDefined(Sym, It->second, Value - It->first);
  }

  for (unsigned I = 0; I != Sections.size(); ++I) {
    // Assign relocations to subsections.
    ArrayRef<any_relocation_info> Relocs{
        reinterpret_cast<const any_relocation_info *>(Buf + Sections[I].reloff),
        Sections[I].nreloc};

    std::map<uint32_t, InputSection *> &Subsec = Subsections[I];

    for (auto RelI = Relocs.begin(), RelE = Relocs.end(); RelI != RelE;
         ++RelI) {
      InputSection::Reloc R;
      uint32_t SecRelOffset;
      if (RelI->r_word0 & R_SCATTERED) {
        R.Type = (RelI->r_word0 >> 24) & 0xf;
        SecRelOffset = RelI->r_word0 & 0xffffff;
        R.HasImplicitAddend = true;

        uint32_t Addr = RelI->r_word1;
        for (unsigned I = 0; I != Sections.size(); ++I) {
          if (Addr >= Sections[I].addr &&
              Addr < Sections[I].addr + Sections[I].size) {
            const section_64 &RelSec = Sections[I];
            std::map<uint32_t, InputSection *> &RelSubsec = Subsections[I];
            auto RelIt = RelSubsec.upper_bound(Addr - RelSec.addr);
            --RelIt;
            assert(RelIt != RelSubsec.end());
            R.Target = RelIt->second;
            R.Addend = Addr - Sections[I].addr;
          }
        }
      } else {
        R.Type = RelI->r_word1 >> 28;
        SecRelOffset = RelI->r_word0;

        if ((RelI->r_word1 >> 27) & 1) {
          R.Target = Syms[RelI->r_word1 & 0xffffff];
          R.HasImplicitAddend = true;
        } else {
          unsigned SecNo = (RelI->r_word1 & 0xffffff) - 1;
          const section_64 &RelSec = Sections[SecNo];
          std::map<uint32_t, InputSection *> &RelSubsec = Subsections[SecNo];
          uint64_t TargetAddr =
              Target->getImplicitAddend(Buf + Sections[I].offset + SecRelOffset,
                                        R.Type) -
              RelSec.addr;
          auto It = RelSubsec.upper_bound(TargetAddr);
          --It;
          assert(It != RelSubsec.end());
          R.Target = It->second;
          R.Addend = TargetAddr - It->first;
          R.HasImplicitAddend = false;
        }
      }

      auto It = Subsec.upper_bound(SecRelOffset);
      --It;
      assert(It != Subsec.end());

      R.Offset = SecRelOffset - It->first;
      It->second->Relocs.push_back(R);
    }

    // Add subsections to output segment.
    OutputSegment *OS = getOrCreateOutputSegment(
        StringRef(Sections[I].segname, strnlen(Sections[I].segname, 16)),
        VM_PROT_READ | VM_PROT_WRITE);

    std::vector<InputSection *> &SectionVec = OS->Sections[StringRef(
        Sections[I].sectname, strnlen(Sections[I].sectname, 16))];
    SectionVec.reserve(Sections.size() + Subsections[I].size());

    for (auto &P : Subsections[I]) {
      InputSection *Sec = P.second;
      SectionVec.push_back(Sec);
    }
  }
}
