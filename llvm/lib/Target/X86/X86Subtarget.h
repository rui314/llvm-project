//=====---- X86Subtarget.h - Define Subtarget for the X86 -----*- C++ -*--====//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the X86 specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef X86SUBTARGET_H
#define X86SUBTARGET_H

#include "llvm/ADT/Triple.h"
#include "llvm/Target/TargetSubtargetInfo.h"
#include "llvm/CallingConv.h"
#include <string>

#define GET_SUBTARGETINFO_HEADER
#include "X86GenSubtargetInfo.inc"

namespace llvm {
class GlobalValue;
class StringRef;
class TargetMachine;

/// PICStyles - The X86 backend supports a number of different styles of PIC.
///
namespace PICStyles {
enum Style {
  StubPIC,          // Used on i386-darwin in -fPIC mode.
  StubDynamicNoPIC, // Used on i386-darwin in -mdynamic-no-pic mode.
  GOT,              // Used on many 32-bit unices in -fPIC mode.
  RIPRel,           // Used on X86-64 when not in -static mode.
  None              // Set when in -static mode (not PIC or DynamicNoPIC mode).
};
}

class X86Subtarget : public X86GenSubtargetInfo {
protected:
  enum X86SSEEnum {
    NoMMXSSE, MMX, SSE1, SSE2, SSE3, SSSE3, SSE41, SSE42
  };

  enum X863DNowEnum {
    NoThreeDNow, ThreeDNow, ThreeDNowA
  };

  /// PICStyle - Which PIC style to use
  ///
  PICStyles::Style PICStyle;

  /// X86SSELevel - MMX, SSE1, SSE2, SSE3, SSSE3, SSE41, SSE42, or
  /// none supported.
  X86SSEEnum X86SSELevel;

  /// X863DNowLevel - 3DNow or 3DNow Athlon, or none supported.
  ///
  X863DNowEnum X863DNowLevel;

  /// HasCMov - True if this processor has conditional move instructions
  /// (generally pentium pro+).
  bool HasCMov;

  /// HasX86_64 - True if the processor supports X86-64 instructions.
  ///
  bool HasX86_64;

  /// HasPOPCNT - True if the processor supports POPCNT.
  bool HasPOPCNT;

  /// HasSSE4A - True if the processor supports SSE4A instructions.
  bool HasSSE4A;

  /// HasAVX - Target has AVX instructions
  bool HasAVX;

  /// HasAVX2 - Target has AVX2 instructions
  bool HasAVX2;

  /// HasAES - Target has AES instructions
  bool HasAES;

  /// HasCLMUL - Target has carry-less multiplication
  bool HasCLMUL;

  /// HasFMA3 - Target has 3-operand fused multiply-add
  bool HasFMA3;

  /// HasFMA4 - Target has 4-operand fused multiply-add
  bool HasFMA4;

  /// HasXOP - Target has XOP instructions
  bool HasXOP;

  /// HasMOVBE - True if the processor has the MOVBE instruction.
  bool HasMOVBE;

  /// HasRDRAND - True if the processor has the RDRAND instruction.
  bool HasRDRAND;

  /// HasF16C - Processor has 16-bit floating point conversion instructions.
  bool HasF16C;

  /// HasFSGSBase - Processor has FS/GS base insturctions.
  bool HasFSGSBase;

  /// HasLZCNT - Processor has LZCNT instruction.
  bool HasLZCNT;

  /// HasBMI - Processor has BMI1 instructions.
  bool HasBMI;

  /// HasBMI2 - Processor has BMI2 instructions.
  bool HasBMI2;

  /// IsBTMemSlow - True if BT (bit test) of memory instructions are slow.
  bool IsBTMemSlow;

  /// IsUAMemFast - True if unaligned memory access is fast.
  bool IsUAMemFast;

  /// HasVectorUAMem - True if SIMD operations can have unaligned memory
  /// operands. This may require setting a feature bit in the processor.
  bool HasVectorUAMem;

  /// HasCmpxchg16b - True if this processor has the CMPXCHG16B instruction;
  /// this is true for most x86-64 chips, but not the first AMD chips.
  bool HasCmpxchg16b;

  /// stackAlignment - The minimum alignment known to hold of the stack frame on
  /// entry to the function and which must be maintained by every function.
  unsigned stackAlignment;

  /// Max. memset / memcpy size that is turned into rep/movs, rep/stos ops.
  ///
  unsigned MaxInlineSizeThreshold;

  /// TargetTriple - What processor and OS we're targeting.
  Triple TargetTriple;

private:
  /// In64BitMode - True if compiling for 64-bit, false for 32-bit.
  bool In64BitMode;

public:

  /// This constructor initializes the data members to match that
  /// of the specified triple.
  ///
  X86Subtarget(const std::string &TT, const std::string &CPU,
               const std::string &FS,
               unsigned StackAlignOverride, bool is64Bit);

  /// getStackAlignment - Returns the minimum alignment known to hold of the
  /// stack frame on entry to the function and which must be maintained by every
  /// function for this subtarget.
  unsigned getStackAlignment() const { return stackAlignment; }

  /// getMaxInlineSizeThreshold - Returns the maximum memset / memcpy size
  /// that still makes it profitable to inline the call.
  unsigned getMaxInlineSizeThreshold() const { return MaxInlineSizeThreshold; }

  /// ParseSubtargetFeatures - Parses features string setting specified
  /// subtarget options.  Definition of function is auto generated by tblgen.
  void ParseSubtargetFeatures(StringRef CPU, StringRef FS);

  /// AutoDetectSubtargetFeatures - Auto-detect CPU features using CPUID
  /// instruction.
  void AutoDetectSubtargetFeatures();

  bool is64Bit() const { return In64BitMode; }

  PICStyles::Style getPICStyle() const { return PICStyle; }
  void setPICStyle(PICStyles::Style Style)  { PICStyle = Style; }

  bool hasCMov() const { return HasCMov; }
  bool hasMMX() const { return X86SSELevel >= MMX; }
  bool hasSSE1() const { return X86SSELevel >= SSE1; }
  bool hasSSE2() const { return X86SSELevel >= SSE2; }
  bool hasSSE3() const { return X86SSELevel >= SSE3; }
  bool hasSSSE3() const { return X86SSELevel >= SSSE3; }
  bool hasSSE41() const { return X86SSELevel >= SSE41; }
  bool hasSSE42() const { return X86SSELevel >= SSE42; }
  bool hasSSE4A() const { return HasSSE4A; }
  bool has3DNow() const { return X863DNowLevel >= ThreeDNow; }
  bool has3DNowA() const { return X863DNowLevel >= ThreeDNowA; }
  bool hasPOPCNT() const { return HasPOPCNT; }
  bool hasAVX() const { return HasAVX; }
  bool hasAVX2() const { return HasAVX2; }
  bool hasXMM() const { return hasSSE1() || hasAVX(); }
  bool hasXMMInt() const { return hasSSE2() || hasAVX(); }
  bool hasSSE3orAVX() const { return hasSSE3() || hasAVX(); }
  bool hasSSSE3orAVX() const { return hasSSSE3() || hasAVX(); }
  bool hasSSE41orAVX() const { return hasSSE41() || hasAVX(); }
  bool hasSSE42orAVX() const { return hasSSE42() || hasAVX(); }
  bool hasAES() const { return HasAES; }
  bool hasCLMUL() const { return HasCLMUL; }
  bool hasFMA3() const { return HasFMA3; }
  bool hasFMA4() const { return HasFMA4; }
  bool hasXOP() const { return HasXOP; }
  bool hasMOVBE() const { return HasMOVBE; }
  bool hasRDRAND() const { return HasRDRAND; }
  bool hasF16C() const { return HasF16C; }
  bool hasFSGSBase() const { return HasFSGSBase; }
  bool hasLZCNT() const { return HasLZCNT; }
  bool hasBMI() const { return HasBMI; }
  bool hasBMI2() const { return HasBMI2; }
  bool isBTMemSlow() const { return IsBTMemSlow; }
  bool isUnalignedMemAccessFast() const { return IsUAMemFast; }
  bool hasVectorUAMem() const { return HasVectorUAMem; }
  bool hasCmpxchg16b() const { return HasCmpxchg16b; }

  const Triple &getTargetTriple() const { return TargetTriple; }

  bool isTargetDarwin() const { return TargetTriple.isOSDarwin(); }
  bool isTargetFreeBSD() const {
    return TargetTriple.getOS() == Triple::FreeBSD;
  }
  bool isTargetSolaris() const {
    return TargetTriple.getOS() == Triple::Solaris;
  }

  // ELF is a reasonably sane default and the only other X86 targets we
  // support are Darwin and Windows. Just use "not those".
  bool isTargetELF() const {
    return !isTargetDarwin() && !isTargetWindows() && !isTargetCygMing();
  }
  bool isTargetLinux() const { return TargetTriple.getOS() == Triple::Linux; }
  bool isTargetNaCl() const {
    return TargetTriple.getOS() == Triple::NativeClient;
  }
  bool isTargetNaCl32() const { return isTargetNaCl() && !is64Bit(); }
  bool isTargetNaCl64() const { return isTargetNaCl() && is64Bit(); }

  bool isTargetWindows() const { return TargetTriple.getOS() == Triple::Win32; }
  bool isTargetMingw() const { return TargetTriple.getOS() == Triple::MinGW32; }
  bool isTargetCygwin() const { return TargetTriple.getOS() == Triple::Cygwin; }
  bool isTargetCygMing() const {
    return isTargetMingw() || isTargetCygwin();
  }

  /// isTargetCOFF - Return true if this is any COFF/Windows target variant.
  bool isTargetCOFF() const {
    return isTargetMingw() || isTargetCygwin() || isTargetWindows();
  }

  bool isTargetWin64() const {
    // FIXME: x86_64-cygwin has not been released yet.
    return In64BitMode && (isTargetCygMing() || isTargetWindows());
  }

  bool isTargetEnvMacho() const {
    return isTargetDarwin() || (TargetTriple.getEnvironment() == Triple::MachO);
  }

  bool isTargetWin32() const {
    return !In64BitMode && (isTargetMingw() || isTargetWindows());
  }

  bool isPICStyleSet() const { return PICStyle != PICStyles::None; }
  bool isPICStyleGOT() const { return PICStyle == PICStyles::GOT; }
  bool isPICStyleRIPRel() const { return PICStyle == PICStyles::RIPRel; }

  bool isPICStyleStubPIC() const {
    return PICStyle == PICStyles::StubPIC;
  }

  bool isPICStyleStubNoDynamic() const {
    return PICStyle == PICStyles::StubDynamicNoPIC;
  }
  bool isPICStyleStubAny() const {
    return PICStyle == PICStyles::StubDynamicNoPIC ||
           PICStyle == PICStyles::StubPIC; }

  /// ClassifyGlobalReference - Classify a global variable reference for the
  /// current subtarget according to how we should reference it in a non-pcrel
  /// context.
  unsigned char ClassifyGlobalReference(const GlobalValue *GV,
                                        const TargetMachine &TM)const;

  /// ClassifyBlockAddressReference - Classify a blockaddress reference for the
  /// current subtarget according to how we should reference it in a non-pcrel
  /// context.
  unsigned char ClassifyBlockAddressReference() const;

  /// IsLegalToCallImmediateAddr - Return true if the subtarget allows calls
  /// to immediate address.
  bool IsLegalToCallImmediateAddr(const TargetMachine &TM) const;

  /// This function returns the name of a function which has an interface
  /// like the non-standard bzero function, if such a function exists on
  /// the current subtarget and it is considered prefereable over
  /// memset with zero passed as the second argument. Otherwise it
  /// returns null.
  const char *getBZeroEntry() const;

  /// getSpecialAddressLatency - For targets where it is beneficial to
  /// backschedule instructions that compute addresses, return a value
  /// indicating the number of scheduling cycles of backscheduling that
  /// should be attempted.
  unsigned getSpecialAddressLatency() const;
};

} // End llvm namespace

#endif
