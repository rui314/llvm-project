//===- Target.h -------------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_MACHO_TARGET_H
#define LLD_MACHO_TARGET_H

#include <cstdint>

namespace lld {
namespace mach_o2 {

class TargetInfo {
public:
  virtual ~TargetInfo() {}
  virtual uint64_t getImplicitAddend(const uint8_t *Loc,
                                     uint8_t Type) const = 0;
  virtual void relocateOne(uint8_t *Loc, uint8_t Type, uint64_t Val) const = 0;

  uint32_t CPUType;
  uint32_t CPUSubtype;
};

TargetInfo *createX86_64TargetInfo();

extern TargetInfo *Target;

} // namespace mach_o2
} // namespace lld

#endif
