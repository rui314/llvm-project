#include "Driver.h"
#include "Config.h"
#include "InputFiles.h"
#include "OutputSegment.h"
#include "SymbolTable.h"
#include "Symbols.h"
#include "Target.h"
#include "Writer.h"

#include "lld/Common/Driver.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/LLVM.h"
#include "lld/Common/Memory.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/Option.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace lld;
using namespace lld::mach_o2;
using namespace llvm;
using namespace llvm::support::endian;
using namespace llvm::MachO;

using llvm::None;
using llvm::Optional;

Configuration *mach_o2::Config;

// Open a give file path and returns it as a memory-mapped file.
static Optional<MemoryBufferRef> readFile(StringRef Path) {
  auto MBOrErr = MemoryBuffer::getFile(Path);
  if (auto EC = MBOrErr.getError()) {
    error("cannot open " + Path + ": " + EC.message());
    return None;
  }

  std::unique_ptr<MemoryBuffer> &MB = *MBOrErr;
  MemoryBufferRef MBRef = MB->getMemBufferRef();
  make<std::unique_ptr<MemoryBuffer>>(std::move(MB)); // take MB ownership

  // If this is a regular non-fat file, return it.
  const char *Buf = MBRef.getBufferStart();
  auto *Hdr = reinterpret_cast<const MachO::fat_header *>(Buf);
  if (read32be(&Hdr->magic) != MachO::FAT_MAGIC)
    return MBRef;

  // Object files and archive files may be fat files, which contains
  // multiple real files for different CPU ISAs. Here, we search for a
  // file that matches with the current link target and returns it as
  // a MemoryBufferRef.
  auto *Arch = reinterpret_cast<const MachO::fat_arch *>(Buf + sizeof(*Hdr));
  for (size_t I = 0; I < Hdr->nfat_arch; ++I) {
    if (read32be(&Arch[I].cputype) != Target->CPUType ||
        read32be(&Arch[I].cpusubtype) != Target->CPUSubtype)
      continue;

    uint32_t Offset = read32be(&Arch[I].offset);
    uint32_t Size = read32be(&Arch[I].size);
    if (Offset + Size > MBRef.getBufferSize())
      error(Path + ": broken file");
    return MemoryBufferRef(StringRef(Buf + Offset, Size), Path);
  }

  return MBRef;
}

// Create OptTable

// Create prefix string literals used in Options.td
#define PREFIX(NAME, VALUE) const char *const NAME[] = VALUE;
#include "Options.inc"
#undef PREFIX

// Create table mapping all options defined in Options.td
static const opt::OptTable::Info OptInfo[] = {
#define OPTION(X1, X2, ID, KIND, GROUP, ALIAS, X7, X8, X9, X10, X11, X12)      \
  {X1, X2, X10,         X11,         OPT_##ID, opt::Option::KIND##Class,       \
   X9, X8, OPT_##GROUP, OPT_##ALIAS, X7,       X12},
#include "Options.inc"
#undef OPTION
};

MachOOptTable::MachOOptTable() : OptTable(OptInfo) {}

opt::InputArgList MachOOptTable::parse(ArrayRef<const char *> Argv) {
  // Make InputArgList from string vectors.
  unsigned MissingIndex;
  unsigned MissingCount;
  SmallVector<const char *, 256> Vec(Argv.data(), Argv.data() + Argv.size());

  opt::InputArgList Args = this->ParseArgs(Vec, MissingIndex, MissingCount);

  if (MissingCount)
    error(Twine(Args.getArgString(MissingIndex)) + ": missing argument");

  for (auto *Arg : Args.filtered(OPT_UNKNOWN))
    error("unknown argument: " + Arg->getSpelling());
  return Args;
}

static TargetInfo *createTargetInfo(opt::InputArgList &Args) {
  StringRef S = Args.getLastArgValue(OPT_arch, "x86_64");
  if (S != "x86_64")
    error("missing or bad -arch");
  return createX86_64TargetInfo();
}

bool mach_o2::link(llvm::ArrayRef<const char *> ArgsArr) {
  MachOOptTable Parser;
  opt::InputArgList Args = Parser.parse(ArgsArr.slice(1));

  Config = make<Configuration>();
  Symtab = make<SymbolTable>();
  Target = createTargetInfo(Args);

  Config->Entry = Symtab->addUndefined(Args.getLastArgValue(OPT_e, "start"));
  Config->OutputFile = Args.getLastArgValue(OPT_o, "a.out");

  getOrCreateOutputSegment("__TEXT", VM_PROT_READ | VM_PROT_EXECUTE);
  getOrCreateOutputSegment("__DATA", VM_PROT_READ | VM_PROT_WRITE);

  std::vector<InputFile *> Files;

  for (auto *Arg : Args) {
    if (Arg->getOption().getID() != OPT_INPUT)
      continue;
    Optional<MemoryBufferRef> Buf = readFile(Arg->getValue());
    if (!Buf)
      return true;
    Files.push_back(createObjectFile(*Buf));
  }

  if (!isa<Defined>(Config->Entry))
    error("undefined symbol: " + Config->Entry->getName());

  // Initialize InputSections.
  for (InputFile *File : Files)
    for (InputSection *Sec : File->Sections)
      InputSections.push_back(Sec);

  // Add input sections to output segments.
  for (InputSection *IS : InputSections) {
    OutputSegment *OS =
        getOrCreateOutputSegment(IS->Name, VM_PROT_READ | VM_PROT_WRITE);
    std::vector<InputSection *> &Vec = OS->Sections[IS->Name];
    Vec.push_back(IS);
  }

  // Write to an output file.
  writeResult();
  return !errorCount();
}
