//===-- FileRemapper.h - File Remapping Helper ------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ARCMIGRATE_FILEREMAPPER_H
#define LLVM_CLANG_ARCMIGRATE_FILEREMAPPER_H

#include "clang/Basic/LLVM.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {
  class MemoryBuffer;
}

namespace clang {
  class FileManager;
  class FileEntry;
  class Diagnostic;
  class CompilerInvocation;

namespace arcmt {

class FileRemapper {
  // FIXME: Reuse the same FileManager for multiple ASTContexts.
  llvm::OwningPtr<FileManager> FileMgr;

  typedef llvm::PointerUnion<const FileEntry *, llvm::MemoryBuffer *> Target;
  typedef llvm::DenseMap<const FileEntry *, Target> MappingsTy;
  MappingsTy FromToMappings;

  llvm::DenseMap<const FileEntry *, const FileEntry *> ToFromMappings;

public:
  FileRemapper();
  ~FileRemapper();
  
  bool initFromDisk(StringRef outputDir, Diagnostic &Diag,
                    bool ignoreIfFilesChanged);
  bool flushToDisk(StringRef outputDir, Diagnostic &Diag);

  bool overwriteOriginal(Diagnostic &Diag,
                         StringRef outputDir = StringRef());

  void remap(StringRef filePath, llvm::MemoryBuffer *memBuf);
  void remap(StringRef filePath, StringRef newPath);

  void applyMappings(CompilerInvocation &CI) const;

  void transferMappingsAndClear(CompilerInvocation &CI);

  void clear(StringRef outputDir = StringRef());

private:
  void remap(const FileEntry *file, llvm::MemoryBuffer *memBuf);
  void remap(const FileEntry *file, const FileEntry *newfile);

  const FileEntry *getOriginalFile(StringRef filePath);
  void resetTarget(Target &targ);

  bool report(const Twine &err, Diagnostic &Diag);

  std::string getRemapInfoFile(StringRef outputDir);
};

} // end namespace arcmt

}  // end namespace clang

#endif
