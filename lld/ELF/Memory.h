//===- Memory.h -------------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines arena allocators.
//
// Almost all large objects, such as files, sections or symbols, are
// used for the entire lifetime of the linker once they are created.
// This usage characteristic makes arena allocator an attractive choice
// where the entire linker is one arena. With an arena, newly created
// objects belong to the arena and freed all at once when everything is done.
// Arena allocators are efficient and easy to understand.
// Most objects are allocated using the arena allocators defined by this file.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_ELF_MEMORY_H
#define LLD_ELF_MEMORY_H

#include "llvm/Support/Allocator.h"
#include "llvm/Support/StringSaver.h"
#include <vector>

namespace lld {
namespace elf {

// Use this arena if your object doesn't have a destructor.
extern llvm::BumpPtrAllocator BAlloc;
extern llvm::StringSaver Saver;

// These two classes are hack to keep track of all
// SpecificBumpPtrAllocator instances.
struct SpecificAllocBase {
  SpecificAllocBase();
  virtual ~SpecificAllocBase() = default;
  virtual void reset() = 0;
  static std::vector<SpecificAllocBase *> Instances;
};

template <class T> struct SpecificAlloc : public SpecificAllocBase {
  void reset() override { Alloc.DestroyAll(); }
  llvm::SpecificBumpPtrAllocator<T> Alloc;
};

// Use this arean if your object have a destructor.
// Your destructor will be invoked from freeArena().
template <class T> static T *alloc() {
  static SpecificAlloc<T> Alloc;
  return Alloc.Alloc.Allocate();
}

void freeArena();
}
}

#endif
