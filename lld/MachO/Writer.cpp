#include "Writer.h"
#include "InputSection.h"
#include "OutputSegment.h"

#include "llvm/BinaryFormat/MachO.h"
#include "lld/Common/ErrorHandler.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm;
using namespace llvm::MachO;

namespace {

struct Writer {
  Writer() : Buffer(errorHandler().OutputBuffer) {}

  std::unique_ptr<FileOutputBuffer> &Buffer;
  uint64_t SizeofCmds;
  uint64_t FileSize;

  void assignAddresses();
  void openFile();
  void writeHeader();
  void writeSections();

  void run();
};

void Writer::assignAddresses() {
  uint64_t Addr = sizeof(mach_header_64) +
                 OutputSegments.size() * sizeof(segment_command_64);
  for (auto &P : OutputSegments)
    Addr += P.second->Sections.size() * sizeof(section_64);
  SizeofCmds = Addr - sizeof(mach_header_64);

  for (auto &Seg : OutputSegments) {
    Addr = alignTo(Addr, 4096);
    for (auto &Sect : Seg.second->Sections) {
      for (InputSection *IS : Sect.second) {
        Addr = alignTo(Addr, IS->Align);
        IS->Addr = Addr;
        Addr += IS->Data.size();
      }
    }
  }

  FileSize = Addr;
}

void Writer::openFile() {
   Expected<std::unique_ptr<FileOutputBuffer>> BufferOrErr =
      FileOutputBuffer::create("a.out", FileSize, 0);

  if (!BufferOrErr)
    error("failed to open a.out: " + llvm::toString(BufferOrErr.takeError()));
  else
    Buffer = std::move(*BufferOrErr);
}

void Writer::writeHeader() {
  auto *MH = reinterpret_cast<mach_header_64 *>(Buffer->getBufferStart());
  MH->magic = MH_MAGIC_64;
  MH->cputype = CPU_TYPE_X86_64;
  MH->cpusubtype = CPU_SUBTYPE_X86_64_ALL;
  MH->filetype = MH_EXECUTE;
  MH->ncmds = OutputSegments.size();
  MH->sizeofcmds = SizeofCmds;

  auto *Hdr = reinterpret_cast<char *>(MH + 1);
  for (auto &Seg : OutputSegments) {
    auto *SegCmd = reinterpret_cast<segment_command_64 *>(Hdr);
    Hdr += sizeof(segment_command_64);

    SegCmd->cmd = LC_SEGMENT_64;
    SegCmd->cmdsize = sizeof(segment_command_64) +
                      Seg.second->Sections.size() * sizeof(section_64);
    memcpy(SegCmd->segname, Seg.first.data(), Seg.first.size());
    InputSection *FirstSec = Seg.second->Sections.front().second[0];
    InputSection *LastSec = Seg.second->Sections.back().second.back();
    SegCmd->vmaddr = SegCmd->fileoff = FirstSec->Addr;
    SegCmd->vmsize = SegCmd->filesize =
        LastSec->Addr + LastSec->Data.size() - FirstSec->Addr;
    SegCmd->maxprot = SegCmd->initprot = Seg.second->Perms;
    SegCmd->nsects = Seg.second->Sections.size();

    for (auto &Sect : Seg.second->Sections) {
      auto *SectHdr = reinterpret_cast<section_64 *>(Hdr);
      Hdr += sizeof(section_64);

      memcpy(SectHdr->sectname, Sect.first.data(), Sect.first.size());
      memcpy(SectHdr->segname, Seg.first.data(), Seg.first.size());

      SectHdr->addr = SectHdr->offset = Sect.second[0]->Addr;
      SectHdr->size = Sect.second.back()->Addr +
                      Sect.second.back()->Data.size() - Sect.second[0]->Addr;
    }
  }
}

void Writer::writeSections() {
  for (auto &Seg : OutputSegments)
    for (auto &Sect : Seg.second->Sections)
      for (InputSection *IS : Sect.second)
        IS->writeTo(Buffer->getBufferStart() + IS->Addr);
}

void Writer::run() {
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
