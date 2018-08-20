//===---------------------- ExecuteStage.cpp --------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file defines the execution stage of an instruction pipeline.
///
/// The ExecuteStage is responsible for managing the hardware scheduler
/// and issuing notifications that an instruction has been executed.
///
//===----------------------------------------------------------------------===//

#include "ExecuteStage.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "llvm-mca"

namespace mca {

using namespace llvm;

HWStallEvent::GenericEventType toHWStallEventType(Scheduler::Status Status) {
  switch (Status) {
  case Scheduler::SC_LOAD_QUEUE_FULL:
    return HWStallEvent::LoadQueueFull;
  case Scheduler::SC_STORE_QUEUE_FULL:
    return HWStallEvent::StoreQueueFull;
  case Scheduler::SC_BUFFERS_FULL:
    return HWStallEvent::SchedulerQueueFull;
  case Scheduler::SC_DISPATCH_GROUP_STALL:
    return HWStallEvent::DispatchGroupStall;
  case Scheduler::SC_AVAILABLE:
    return HWStallEvent::Invalid;
  }

  llvm_unreachable("Don't know how to process this StallKind!");
}

bool ExecuteStage::isAvailable(const InstRef &IR) const {
  if (Scheduler::Status S = HWS.isAvailable(IR)) {
    HWStallEvent::GenericEventType ET = toHWStallEventType(S);
    notifyEvent<HWStallEvent>(HWStallEvent(ET, IR));
    return false;
  }

  return true;
}

void ExecuteStage::reclaimSchedulerResources() {
  SmallVector<ResourceRef, 8> ResourcesFreed;
  HWS.reclaimSimulatedResources(ResourcesFreed);
  for (const ResourceRef &RR : ResourcesFreed)
    notifyResourceAvailable(RR);
}

Error ExecuteStage::updateSchedulerQueues() {
  SmallVector<InstRef, 4> InstructionIDs;
  HWS.updateIssuedSet(InstructionIDs);
  for (InstRef &IR : InstructionIDs) {
    notifyInstructionExecuted(IR);
    //FIXME: add a buffer of executed instructions.
    if (Error S = moveToTheNextStage(IR))
      return S;
  }
  InstructionIDs.clear();

  HWS.updatePendingQueue(InstructionIDs);
  for (const InstRef &IR : InstructionIDs)
    notifyInstructionReady(IR);
  return ErrorSuccess();
}

Error ExecuteStage::issueReadyInstructions() {
  SmallVector<InstRef, 4> InstructionIDs;
  InstRef IR = HWS.select();
  while (IR.isValid()) {
    SmallVector<std::pair<ResourceRef, double>, 4> Used;
    HWS.issueInstruction(IR, Used);

    // Reclaim instruction resources and perform notifications.
    const InstrDesc &Desc = IR.getInstruction()->getDesc();
    notifyReleasedBuffers(Desc.Buffers);
    notifyInstructionIssued(IR, Used);
    if (IR.getInstruction()->isExecuted()) {
      notifyInstructionExecuted(IR);
      //FIXME: add a buffer of executed instructions.
      if (Error S = moveToTheNextStage(IR))
        return S;
    }

    // Instructions that have been issued during this cycle might have unblocked
    // other dependent instructions. Dependent instructions may be issued during
    // this same cycle if operands have ReadAdvance entries.  Promote those
    // instructions to the ReadySet and tell to the caller that we need
    // another round of 'issue()'.
    HWS.promoteToReadySet(InstructionIDs);
    for (const InstRef &I : InstructionIDs)
      notifyInstructionReady(I);
    InstructionIDs.clear();

    // Select the next instruction to issue.
    IR = HWS.select();
  }

  return ErrorSuccess();
}

// The following routine is the maintenance routine of the ExecuteStage.
// It is responsible for updating the hardware scheduler (HWS), including
// reclaiming the HWS's simulated hardware resources, as well as updating the
// HWS's queues.
//
// This routine also processes the instructions that are ready for issuance.
// These instructions are managed by the HWS's ready queue and can be accessed
// via the Scheduler::select() routine.
//
// Notifications are issued to this stage's listeners when instructions are
// moved between the HWS's queues.  In particular, when an instruction becomes
// ready or executed.
Error ExecuteStage::cycleStart() {
  reclaimSchedulerResources();
  if (Error S = updateSchedulerQueues())
    return S;
  return issueReadyInstructions();
}

// Schedule the instruction for execution on the hardware.
Error ExecuteStage::execute(InstRef &IR) {
  assert(isAvailable(IR) && "Scheduler is not available!");

#ifndef NDEBUG
  // Ensure that the HWS has not stored this instruction in its queues.
  HWS.sanityCheck(IR);
#endif
  // Reserve a slot in each buffered resource. Also, mark units with
  // BufferSize=0 as reserved. Resources with a buffer size of zero will only
  // be released after MCIS is issued, and all the ResourceCycles for those
  // units have been consumed.
  const InstrDesc &Desc = IR.getInstruction()->getDesc();
  HWS.dispatch(IR);
  notifyReservedBuffers(Desc.Buffers);
  if (!HWS.isReady(IR))
    return ErrorSuccess();

  // If we did not return early, then the scheduler is ready for execution.
  notifyInstructionReady(IR);

  // If we cannot issue immediately, the HWS will add IR to its ready queue for
  // execution later, so we must return early here.
  if (!HWS.mustIssueImmediately(IR))
    return ErrorSuccess();

  // Issue IR to the underlying pipelines.
  SmallVector<std::pair<ResourceRef, double>, 4> Used;
  HWS.issueInstruction(IR, Used);

  // Perform notifications.
  notifyReleasedBuffers(Desc.Buffers);
  notifyInstructionIssued(IR, Used);
  if (IR.getInstruction()->isExecuted()) {
    notifyInstructionExecuted(IR);
    //FIXME: add a buffer of executed instructions.
    return moveToTheNextStage(IR);
  }
  return ErrorSuccess();
}

void ExecuteStage::notifyInstructionExecuted(const InstRef &IR) {
  LLVM_DEBUG(dbgs() << "[E] Instruction Executed: #" << IR << '\n');
  notifyEvent<HWInstructionEvent>(
      HWInstructionEvent(HWInstructionEvent::Executed, IR));
}

void ExecuteStage::notifyInstructionReady(const InstRef &IR) {
  LLVM_DEBUG(dbgs() << "[E] Instruction Ready: #" << IR << '\n');
  notifyEvent<HWInstructionEvent>(
      HWInstructionEvent(HWInstructionEvent::Ready, IR));
}

void ExecuteStage::notifyResourceAvailable(const ResourceRef &RR) {
  LLVM_DEBUG(dbgs() << "[E] Resource Available: [" << RR.first << '.'
                    << RR.second << "]\n");
  for (HWEventListener *Listener : getListeners())
    Listener->onResourceAvailable(RR);
}

void ExecuteStage::notifyInstructionIssued(
    const InstRef &IR, ArrayRef<std::pair<ResourceRef, double>> Used) {
  LLVM_DEBUG({
    dbgs() << "[E] Instruction Issued: #" << IR << '\n';
    for (const std::pair<ResourceRef, unsigned> &Resource : Used) {
      dbgs() << "[E] Resource Used: [" << Resource.first.first << '.'
             << Resource.first.second << "], ";
      dbgs() << "cycles: " << Resource.second << '\n';
    }
  });
  notifyEvent<HWInstructionEvent>(HWInstructionIssuedEvent(IR, Used));
}

void ExecuteStage::notifyReservedBuffers(ArrayRef<uint64_t> Buffers) {
  if (Buffers.empty())
    return;

  SmallVector<unsigned, 4> BufferIDs(Buffers.begin(), Buffers.end());
  std::transform(Buffers.begin(), Buffers.end(), BufferIDs.begin(),
                 [&](uint64_t Op) { return HWS.getResourceID(Op); });
  for (HWEventListener *Listener : getListeners())
    Listener->onReservedBuffers(BufferIDs);
}

void ExecuteStage::notifyReleasedBuffers(ArrayRef<uint64_t> Buffers) {
  if (Buffers.empty())
    return;

  SmallVector<unsigned, 4> BufferIDs(Buffers.begin(), Buffers.end());
  std::transform(Buffers.begin(), Buffers.end(), BufferIDs.begin(),
                 [&](uint64_t Op) { return HWS.getResourceID(Op); });
  for (HWEventListener *Listener : getListeners())
    Listener->onReleasedBuffers(BufferIDs);
}

} // namespace mca
