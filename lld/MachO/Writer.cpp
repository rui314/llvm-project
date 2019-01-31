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
#include "llvm/Support/MathExtras.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm;
using namespace llvm::MachO;
using namespace llvm::support;

namespace {
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
  uint64_t SizeofCmds;
  uint64_t FileSize;

  void createLoadCommands();
  void assignAddresses();
  void openFile();
  void writeHeader();
  void writeSections();

  void run();
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

class LCHeaderSegment : public LoadCommand {
public:
  LCHeaderSegment(uint64_t &SizeofCmds) : SizeofCmds(SizeofCmds) {}

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

private:
  uint64_t &SizeofCmds;
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
    return sizeof(dylib_command) + alignTo(Path.size() + 1, 8);
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
} // namespace

void Writer::createLoadCommands() {
  LoadCommands.push_back(make<LCPagezeroSegment>());
  LoadCommands.push_back(make<LCHeaderSegment>(SizeofCmds));

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

  SizeofCmds = 0;
  for (LoadCommand *LC : LoadCommands)
    SizeofCmds += LC->getSize();
  Addr += SizeofCmds;

  for (OutputSegment *Seg : OutputSegments) {
    Addr = alignTo(Addr, PageSize);

    for (auto &P : Seg->Sections) {
      std::vector<InputSection *> Sections = P.second;
      for (InputSection *IS : Sections) {
        Addr = alignTo(Addr, 1 << IS->Align);
        IS->Addr = Addr;
        Addr += IS->Data.size();
      }
    }
  }

  FileSize = alignTo(Addr - ImageBase, PageSize);
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
  Hdr->cpusubtype = CPU_SUBTYPE_X86_64_ALL;
  Hdr->filetype = MH_EXECUTE;
  Hdr->ncmds = LoadCommands.size();
  Hdr->sizeofcmds = SizeofCmds;
  Hdr->flags = MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_PIE;

  uint8_t *P = reinterpret_cast<uint8_t *>(Hdr + 1);
  for (LoadCommand *LC : LoadCommands) {
    LC->writeTo(P);
    P += LC->getSize();
  }
}

void Writer::writeSections() {
  for (OutputSegment *Seg : OutputSegments)
    for (auto &Sect : Seg->Sections)
      for (InputSection *IS : Sect.second)
        IS->writeTo(Buffer->getBufferStart() + IS->Addr - ImageBase);
}

void Writer::run() {
  createLoadCommands();
  assignAddresses();
  openFile();
  if (errorCount())
    return;

  writeHeader();
  writeSections();

  if (auto E = Buffer->commit())
    error("failed to write to the output file: " + toString(std::move(E)));
}

void mach_o2::writeResult() { Writer().run(); }
