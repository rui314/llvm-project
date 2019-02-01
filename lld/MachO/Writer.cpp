#include "Writer.h"
#include "Config.h"
#include "InputSection.h"
#include "InputFiles.h"
#include "OutputSegment.h"
#include "SymbolTable.h"
#include "Symbols.h"

#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/MathExtras.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm;
using namespace llvm::MachO;
using namespace llvm::support;

namespace {
class LCHeaderSegment;
class LCLinkEditSegment;
class LCDyldInfoSegment;

class LoadCommand {
public:
  virtual ~LoadCommand() {}
  virtual uint64_t getSize() = 0;
  virtual void writeTo(uint8_t *Buf) = 0;
};

class Writer {
public:
  Writer() : Buffer(errorHandler().OutputBuffer) {}

  std::vector<LoadCommand *> LoadCommands;

  std::unique_ptr<FileOutputBuffer> &Buffer;
  uint64_t FileSize;

  void createLoadCommands();
  void assignAddresses();
  void createLinkEditContents();
  void openFile();
  void writeHeader();
  void writeSections();

  void run();

  LCHeaderSegment *HeaderSeg = nullptr;
  LCLinkEditSegment *LinkEditSeg = nullptr;
  LCDyldInfoSegment *DyldInfoSeg = nullptr;
};

enum {
  PageSize = 4096,
  ImageBase = 4096,
};

class LCPagezeroSegment : public LoadCommand {
public:
  uint64_t getSize() { return sizeof(segment_command_64); }

  void writeTo(uint8_t *Buf) {
    auto *C = reinterpret_cast<segment_command_64 *>(Buf);
    C->cmd = LC_SEGMENT_64;
    C->cmdsize = getSize();
    strcpy(C->segname, "__PAGEZERO");
    C->vmsize = PageSize;
  }
};

class LCLinkEditSegment : public LoadCommand {
public:
  uint64_t getSize() { return sizeof(segment_command_64); }

  void writeTo(uint8_t *Buf) {
    auto *C = reinterpret_cast<segment_command_64 *>(Buf);
    C->cmd = LC_SEGMENT_64;
    C->cmdsize = getSize();
    strcpy(C->segname, "__LINKEDIT");
    C->fileoff = FileOff;
    C->filesize = Contents.size();
    C->maxprot = VM_PROT_READ | VM_PROT_WRITE;
    C->initprot = VM_PROT_READ;
  }

  uint64_t FileOff = 0;

  SmallVector<char, 128> Contents;
  raw_svector_ostream OS{Contents};
};

class LCHeaderSegment : public LoadCommand {
public:
  uint64_t getSize() { return sizeof(segment_command_64); }

  void writeTo(uint8_t *Buf) {
    auto *C = reinterpret_cast<segment_command_64 *>(Buf);

    C->cmd = LC_SEGMENT_64;
    C->cmdsize = getSize();
    strcpy(C->segname, "__HEADER");
    C->vmaddr = ImageBase;
    C->vmsize = alignTo(sizeof(mach_header_64) + SizeofCmds, PageSize);
    C->filesize = C->vmsize;
    C->maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
    C->initprot = VM_PROT_READ | VM_PROT_EXECUTE;
  }

  uint64_t SizeofCmds;
};

class LCDyldInfoSegment : public LoadCommand {
public:
  uint64_t getSize() { return sizeof(dyld_info_command); }

  void writeTo(uint8_t *Buf) {
    auto *C = reinterpret_cast<dyld_info_command *>(Buf);
    C->cmd = LC_DYLD_INFO_ONLY;
    C->cmdsize = getSize();
    C->export_off = ExportOff;
    C->export_size = ExportSize;
  }

  uint64_t ExportOff = 0;
  uint64_t ExportSize = 0;
};

class LCSegment : public LoadCommand {
public:
  LCSegment(StringRef Name, OutputSegment *Seg) : Name(Name), Seg(Seg) {}

  uint64_t getSize() {
    return sizeof(segment_command_64) +
           Seg->Sections.size() * sizeof(section_64);
  }

  void writeTo(uint8_t *Buf) {
    auto *C = reinterpret_cast<segment_command_64 *>(Buf);
    Buf += sizeof(segment_command_64);

    C->cmd = LC_SEGMENT_64;
    C->cmdsize = getSize();
    memcpy(C->segname, Name.data(), Name.size());

    InputSection *FirstSec = Seg->Sections.front().second[0];
    InputSection *LastSec = Seg->Sections.back().second.back();

    C->vmaddr = FirstSec->Addr;
    C->fileoff = FirstSec->Addr - ImageBase;
    C->vmsize = C->filesize =
        LastSec->Addr + LastSec->Data.size() - FirstSec->Addr;
    C->maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
    C->initprot = Seg->Perms;
    C->nsects = Seg->Sections.size();

    for (auto &P : Seg->Sections) {
      StringRef S = P.first;
      std::vector<InputSection *> &Sections = P.second;

      auto *SectHdr = reinterpret_cast<section_64 *>(Buf);
      Buf += sizeof(section_64);

      memcpy(SectHdr->sectname, S.data(), S.size());
      memcpy(SectHdr->segname, Name.data(), Name.size());

      SectHdr->addr = Sections[0]->Addr;
      SectHdr->offset = Sections[0]->Addr - ImageBase;
      SectHdr->size = Sections.back()->Addr + Sections.back()->Data.size() -
                      Sections[0]->Addr;
    }
  }

private:
  StringRef Name;
  OutputSegment *Seg;
};

class LCUnixthread : public LoadCommand {
public:
  uint64_t getSize() { return sizeof(thread_command) + 8 + SizeofThreadState; }

  void writeTo(uint8_t *Buf) {
    auto *C = reinterpret_cast<thread_command *>(Buf);
    Buf += sizeof(thread_command);

    C->cmd = LC_UNIXTHREAD;
    C->cmdsize = getSize();

    auto *ThreadStateHdr = reinterpret_cast<ulittle32_t *>(Buf);
    ThreadStateHdr[0] = ThreadStateFlavor;
    ThreadStateHdr[1] = SizeofThreadState / 4;

    auto *ThreadStatePC =
        reinterpret_cast<ulittle64_t *>(Buf + 8 + OffsetofThreadStatePC);
    *ThreadStatePC = Config->Entry->getVA();
  }

private:
  enum {
    ThreadStateFlavor = 4,
    SizeofThreadState = 168,
    OffsetofThreadStatePC = 128,
  };
};

class LCLoadDylib : public LoadCommand {
public:
  LCLoadDylib(StringRef Path) : Path(Path) {}

  uint64_t getSize() {
    return alignTo(sizeof(dylib_command) + Path.size() + 1, 8);
  }

  void writeTo(uint8_t *Buf) {
    auto *C = reinterpret_cast<dylib_command *>(Buf);
    Buf += sizeof(dylib_command);

    C->cmd = LC_LOAD_DYLIB;
    C->cmdsize = getSize();
    C->dylib.name = sizeof(dylib_command);

    memcpy(Buf, Path.data(), Path.size());
    Buf[Path.size()] = '\0';
  }

private:
  StringRef Path;
};

class LCLoadDylinker : public LoadCommand {
public:
  uint64_t getSize() {
    return alignTo(sizeof(dylinker_command) + Path.size() + 1, 8);
  }

  void writeTo(uint8_t *Buf) {
    auto *C = reinterpret_cast<dylinker_command *>(Buf);
    Buf += sizeof(dylinker_command);

    C->cmd = LC_LOAD_DYLINKER;
    C->cmdsize = getSize();
    C->name = sizeof(dylinker_command);

    memcpy(Buf, Path.data(), Path.size());
    Buf[Path.size()] = '\0';
  }

private:
  StringRef Path = "/usr/lib/dyld";
};
} // namespace

void Writer::createLoadCommands() {
  HeaderSeg = make<LCHeaderSegment>();
  LinkEditSeg = make<LCLinkEditSegment>();
  DyldInfoSeg = make<LCDyldInfoSegment>();

  LoadCommands.push_back(make<LCPagezeroSegment>());
  LoadCommands.push_back(HeaderSeg);
  LoadCommands.push_back(LinkEditSeg);
  LoadCommands.push_back(DyldInfoSeg);
  LoadCommands.push_back(make<LCLoadDylinker>());

  for (OutputSegment *Seg : OutputSegments)
    if (!Seg->Sections.empty())
      LoadCommands.push_back(make<LCSegment>(Seg->Name, Seg));

  for (InputFile *File : InputFiles)
    if (!File->DylibName.empty())
      LoadCommands.push_back(make<LCLoadDylib>(File->DylibName));

  LoadCommands.push_back(make<LCUnixthread>());
}

void Writer::assignAddresses() {
  uint64_t Addr = ImageBase + sizeof(mach_header_64);

  uint64_t Size = 0;
  for (LoadCommand *LC : LoadCommands)
    Size += LC->getSize();
  HeaderSeg->SizeofCmds = Size;
  Addr += Size;

  for (OutputSegment *Seg : OutputSegments) {
    Addr = alignTo(Addr, PageSize);

    for (auto &P : Seg->Sections) {
      ArrayRef<InputSection *> Sections = P.second;
      for (InputSection *IS : Sections) {
        Addr = alignTo(Addr, IS->Align);
        IS->Addr = Addr;
        Addr += IS->Data.size();
      }
    }
  }

  LinkEditSeg->FileOff = Addr - ImageBase;
}

void Writer::createLinkEditContents() {
  raw_svector_ostream &OS = LinkEditSeg->OS;

  // Build an export symbol trie that contains only `_main`.
  StringRef SymName = Config->Entry->getName();
  uint64_t Addr = Config->Entry->getVA();

  OS << (char)0;  // Indicates non-leaf node
  OS << (char)1;  // # of child -- we only have `_main`
  OS << SymName << '\0';
  encodeULEB128(SymName.size() + 4, OS);

  // Leaf node
  OS << (char)(1 + getULEB128Size(Addr)); // Node length
  OS << (char)0;			  // Flags
  encodeULEB128(Addr, OS);		  // Address
  OS << (char)0;			  // Terminator

  DyldInfoSeg->ExportOff = LinkEditSeg->FileOff;
  DyldInfoSeg->ExportSize = LinkEditSeg->Contents.size();

  FileSize = LinkEditSeg->FileOff + LinkEditSeg->Contents.size();
}

void Writer::openFile() {
  Expected<std::unique_ptr<FileOutputBuffer>> BufferOrErr =
      FileOutputBuffer::create(Config->OutputFile, FileSize,
                               FileOutputBuffer::F_executable);

  if (!BufferOrErr)
    error("failed to open " + Config->OutputFile + ": " +
          llvm::toString(BufferOrErr.takeError()));
  else
    Buffer = std::move(*BufferOrErr);
}

void Writer::writeHeader() {
  auto *Hdr = reinterpret_cast<mach_header_64 *>(Buffer->getBufferStart());
  Hdr->magic = MH_MAGIC_64;
  Hdr->cputype = CPU_TYPE_X86_64;
  Hdr->cpusubtype = CPU_SUBTYPE_X86_64_ALL | CPU_SUBTYPE_LIB64;
  Hdr->filetype = MH_EXECUTE;
  Hdr->ncmds = LoadCommands.size();
  Hdr->sizeofcmds = HeaderSeg->SizeofCmds;
  Hdr->flags = MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL;

  uint8_t *P = reinterpret_cast<uint8_t *>(Hdr + 1);
  for (LoadCommand *LC : LoadCommands) {
    LC->writeTo(P);
    P += LC->getSize();
  }
}

void Writer::writeSections() {
  uint8_t *Buf = Buffer->getBufferStart();

  for (OutputSegment *Seg : OutputSegments)
    for (auto &Sect : Seg->Sections)
      for (InputSection *IS : Sect.second)
        IS->writeTo(Buf + IS->Addr - ImageBase);

  memcpy(Buf + LinkEditSeg->FileOff, LinkEditSeg->Contents.data(),
	 LinkEditSeg->Contents.size());
}

void Writer::run() {
  createLoadCommands();
  assignAddresses();
  createLinkEditContents();
  openFile();
  if (errorCount())
    return;

  writeHeader();
  writeSections();

  if (auto E = Buffer->commit())
    error("failed to write to the output file: " + toString(std::move(E)));
}

void mach_o2::writeResult() { Writer().run(); }
