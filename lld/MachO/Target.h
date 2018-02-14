#pragma once

#include <cstdint>

namespace lld {
namespace mach_o2 {

class TargetInfo {
public:
  virtual ~TargetInfo() {}
  virtual uint64_t getImplicitAddend(const uint8_t *Loc, uint8_t Type) const = 0;
  virtual void relocateOne(uint8_t *Loc, uint8_t Type, uint64_t Val) const = 0;
};

TargetInfo *createX86_64TargetInfo();

extern TargetInfo *Target;

}
}
