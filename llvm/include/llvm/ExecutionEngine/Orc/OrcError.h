//===------ OrcError.h - Reject symbol lookup requests ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//   Define an error category, error codes, and helper utilities for Orc.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_ORCERROR_H
#define LLVM_EXECUTIONENGINE_ORC_ORCERROR_H

#include "llvm/Support/Error.h"
#include <system_error>

namespace llvm {
namespace orc {

enum class OrcErrorCode : int {
  // RPC Errors
  RemoteAllocatorDoesNotExist = 1,
  RemoteAllocatorIdAlreadyInUse,
  RemoteMProtectAddrUnrecognized,
  RemoteIndirectStubsOwnerDoesNotExist,
  RemoteIndirectStubsOwnerIdAlreadyInUse,
  RPCResponseAbandoned,
  UnexpectedRPCCall,
  UnexpectedRPCResponse,
  UnknownRPCFunction
};

std::error_code orcError(OrcErrorCode ErrCode);

class RPCFunctionNotSupported : public ErrorInfo<RPCFunctionNotSupported> {
public:
  static char ID;

  RPCFunctionNotSupported(std::string RPCFunctionSignature);
  std::error_code convertToErrorCode() const override;
  void log(raw_ostream &OS) const override;
  const std::string &getFunctionSignature() const;
private:
  std::string RPCFunctionSignature;
};

} // End namespace orc.
} // End namespace llvm.

#endif // LLVM_EXECUTIONENGINE_ORC_ORCERROR_H
