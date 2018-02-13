#include "Writer.h"
#include "Config.h"
#include "InputSection.h"
#include "OutputSegment.h"
#include "SymbolTable.h"
#include "Symbols.h"

#include "llvm/BinaryFormat/MachO.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"
#include "llvm/Support/Endian.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm;
using namespace llvm::MachO;
using namespace llvm::support;

namespace {

struct LoadCommand {
  virtual ~LoadCommand() {}
  virtual uint64_t getSize() = 0;
  virtual void writeTo(uint8_t *Buf) = 0;
};

struct Writer {
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
  ImageBase = PageSize,
};

struct LCPagezeroSegment : LoadCommand {
  uint64_t getSize() {
    return sizeof(segment_command_64);
  }

  void writeTo(uint8_t *Buf) {
    auto *SegCmd = reinterpret_cast<segment_command_64 *>(Buf);

    SegCmd->cmd = LC_SEGMENT_64;
    SegCmd->cmdsize = sizeof(segment_command_64);
    strcpy(SegCmd->segname, "__PAGEZERO");
    SegCmd->vmsize = PageSize;
  }
};

struct LCHeaderSegment : LoadCommand {
  LCHeaderSegment(uint64_t &SizeofCmds) : SizeofCmds(SizeofCmds) {}
  uint64_t &SizeofCmds;

  uint64_t getSize() {
    return sizeof(segment_command_64);
  }

  void writeTo(uint8_t *Buf) {
    auto *SegCmd = reinterpret_cast<segment_command_64 *>(Buf);

    SegCmd->cmd = LC_SEGMENT_64;
    SegCmd->cmdsize = sizeof(segment_command_64);
    strcpy(SegCmd->segname, "__HEADER");
    SegCmd->vmaddr = ImageBase;
    SegCmd->vmsize = SegCmd->filesize =
        alignTo(sizeof(mach_header_64) + SizeofCmds, PageSize);
    SegCmd->maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
    SegCmd->initprot = VM_PROT_READ | VM_PROT_EXECUTE;
  }
};

struct LCSegment : LoadCommand {
  StringRef Name;
  OutputSegment *Seg;

  LCSegment(StringRef Name, OutputSegment *Seg) : Name(Name), Seg(Seg) {}

  uint64_t getSize() {
    return sizeof(segment_command_64) +
           Seg->Sections.size() * sizeof(section_64);
  }

  void writeTo(uint8_t *Buf) {
    auto *SegCmd = reinterpret_cast<segment_command_64 *>(Buf);
    Buf += sizeof(segment_command_64);

    SegCmd->cmd = LC_SEGMENT_64;
    SegCmd->cmdsize =
        sizeof(segment_command_64) + Seg->Sections.size() * sizeof(section_64);
    memcpy(SegCmd->segname, Name.data(), Name.size());
    InputSection *FirstSec = Seg->Sections.front().second[0];
    InputSection *LastSec = Seg->Sections.back().second.back();
    SegCmd->vmaddr = FirstSec->Addr;
    SegCmd->fileoff = FirstSec->Addr - ImageBase;
    SegCmd->vmsize = SegCmd->filesize =
        LastSec->Addr + LastSec->Data.size() - FirstSec->Addr;
    SegCmd->maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
    SegCmd->initprot = Seg->Perms;
    SegCmd->nsects = Seg->Sections.size();

    for (auto &Sect : Seg->Sections) {
      auto *SectHdr = reinterpret_cast<section_64 *>(Buf);
      Buf += sizeof(section_64);

      memcpy(SectHdr->sectname, Sect.first.data(), Sect.first.size());
      memcpy(SectHdr->segname, Name.data(), Name.size());

      SectHdr->addr = Sect.second[0]->Addr;
      SectHdr->offset = Sect.second[0]->Addr - ImageBase;
      SectHdr->size = Sect.second.back()->Addr +
                      Sect.second.back()->Data.size() - Sect.second[0]->Addr;
    }
  }
};

struct LCUnixthread : LoadCommand {
  enum {
    ThreadStateFlavor = 4,
    SizeofThreadState = 168,
    OffsetofThreadStatePC = 128,
  };

  uint64_t getSize() {
    return sizeof(thread_command) + 8 + SizeofThreadState;
  }

  void writeTo(uint8_t *Buf) {
    auto *ThrCmd = reinterpret_cast<thread_command *>(Buf);
    Buf += sizeof(thread_command);

    ThrCmd->cmd = LC_UNIXTHREAD;
    ThrCmd->cmdsize = sizeof(thread_command) + 8 + SizeofThreadState;

    auto *ThreadStateHdr = reinterpret_cast<ulittle32_t *>(Buf);
    ThreadStateHdr[0] = ThreadStateFlavor;
    ThreadStateHdr[1] = SizeofThreadState / 4;

    auto *ThreadStatePC =
        reinterpret_cast<ulittle64_t *>(Buf + 8 + OffsetofThreadStatePC);
    *ThreadStatePC = Config->Entry->getVA();
  }
};

void Writer::createLoadCommands() {
  LoadCommands.push_back(make<LCPagezeroSegment>());
  LoadCommands.push_back(make<LCHeaderSegment>(SizeofCmds));
  for (auto &Seg : OutputSegments)
    LoadCommands.push_back(make<LCSegment>(Seg.first, Seg.second));
  LoadCommands.push_back(make<LCUnixthread>());
}

void Writer::assignAddresses() {
  uint64_t Addr = ImageBase + sizeof(mach_header_64);
  for (auto *LC : LoadCommands)
    Addr += LC->getSize();
  SizeofCmds = Addr - sizeof(mach_header_64) - ImageBase;

  for (auto &Seg : OutputSegments) {
    Addr = alignTo(Addr, PageSize);
    for (auto &Sect : Seg.second->Sections) {
      for (InputSection *IS : Sect.second) {
        Addr = alignTo(Addr, IS->Align);
        IS->Addr = Addr;
        Addr += IS->Data.size();
      }
    }
  }

  FileSize = alignTo(Addr - ImageBase, PageSize);
}

void Writer::openFile() {
   Expected<std::unique_ptr<FileOutputBuffer>> BufferOrErr =
      FileOutputBuffer::create(Config->OutputFile, FileSize, FileOutputBuffer::F_executable);

  if (!BufferOrErr)
    error("failed to open " + Config->OutputFile + ": " +
          llvm::toString(BufferOrErr.takeError()));
  else
    Buffer = std::move(*BufferOrErr);
}

void Writer::writeHeader() {
  auto *MH = reinterpret_cast<mach_header_64 *>(Buffer->getBufferStart());
  MH->magic = MH_MAGIC_64;
  MH->cputype = CPU_TYPE_X86_64;
  MH->cpusubtype = CPU_SUBTYPE_X86_64_ALL;
  MH->filetype = MH_EXECUTE;
  MH->ncmds = LoadCommands.size();
  MH->sizeofcmds = SizeofCmds;

  auto *Hdr = reinterpret_cast<uint8_t *>(MH + 1);
  for (auto *LC : LoadCommands) {
    LC->writeTo(Hdr);
    Hdr += LC->getSize();
  }
}

void Writer::writeSections() {
  for (auto &Seg : OutputSegments)
    for (auto &Sect : Seg.second->Sections)
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
}

void mach_o2::writeResult() {
  Writer().run();
}
