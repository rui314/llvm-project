//===- OutputSegment.h ------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_MACHO_OUTPUT_SEGMENT_H
#define LLD_MACHO_OUTPUT_SEGMENT_H

#include "lld/Common/LLVM.h"
#include "llvm/ADT/MapVector.h"

namespace lld {
namespace mach_o2 {

struct InputSection;

struct OutputSegment {
  StringRef Name;
  uint32_t Perms;
  llvm::MapVector<StringRef, std::vector<InputSection *>> Sections;
};

extern std::vector<OutputSegment *> OutputSegments;

OutputSegment *getOrCreateOutputSegment(StringRef Name, uint32_t Perms);

} // namespace mach_o2
} // namespace lld

#endif
