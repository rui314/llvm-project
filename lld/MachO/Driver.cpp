#include "lld/Common/Driver.h"
#include "lld/Common/LLVM.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/BinaryFormat/MachO.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"
#include "InputFiles.h"
#include "OutputSegment.h"
#include "SymbolTable.h"
#include "Writer.h"

using namespace lld;
using namespace llvm::MachO;

using llvm::Optional;
using llvm::None;

static Optional<MemoryBufferRef> readFile(StringRef Path) {
  auto MBOrErr = MemoryBuffer::getFile(Path);
  if (auto EC = MBOrErr.getError()) {
    error("cannot open " + Path + ": " + EC.message());
    return None;
  }

  std::unique_ptr<MemoryBuffer> &MB = *MBOrErr;
  MemoryBufferRef MBRef = MB->getMemBufferRef();
  make<std::unique_ptr<MemoryBuffer>>(std::move(MB)); // take MB ownership
  return MBRef;
}

bool mach_o2::link(llvm::ArrayRef<const char *> Args) {
  Symtab = make<SymbolTable>();

  getOrCreateOutputSegment("__TEXT", VM_PROT_READ | VM_PROT_EXECUTE);
  getOrCreateOutputSegment("__DATA", VM_PROT_READ | VM_PROT_WRITE);

  std::vector<InputFile *> Files;
  for (StringRef Path : Args.slice(1)) {
    Optional<MemoryBufferRef> Buffer = readFile(Path);
    if (!Buffer)
      return true;

    Files.push_back(createObjectFile(*Buffer));
  }

  writeResult();

  return false;
}
