//===-- SBThread.cpp --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/API/SBThread.h"

#include "lldb/API/SBSymbolContext.h"
#include "lldb/API/SBFileSpec.h"
#include "lldb/API/SBStream.h"
#include "lldb/Breakpoint/BreakpointLocation.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/Stream.h"
#include "lldb/Core/StreamFile.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/Process.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/CompileUnit.h"
#include "lldb/Target/StopInfo.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/ThreadPlan.h"
#include "lldb/Target/ThreadPlanStepInstruction.h"
#include "lldb/Target/ThreadPlanStepOut.h"
#include "lldb/Target/ThreadPlanStepRange.h"
#include "lldb/Target/ThreadPlanStepInRange.h"


#include "lldb/API/SBAddress.h"
#include "lldb/API/SBDebugger.h"
#include "lldb/API/SBFrame.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBValue.h"

using namespace lldb;
using namespace lldb_private;

//----------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------
SBThread::SBThread () :
    m_opaque_wp ()
{
}

SBThread::SBThread (const ThreadSP& lldb_object_sp) :
    m_opaque_wp (lldb_object_sp)
{
}

SBThread::SBThread (const SBThread &rhs) :
    m_opaque_wp (rhs.m_opaque_wp)
{
}

//----------------------------------------------------------------------
// Assignment operator
//----------------------------------------------------------------------

const lldb::SBThread &
SBThread::operator = (const SBThread &rhs)
{
    if (this != &rhs)
        m_opaque_wp = rhs.m_opaque_wp;
    return *this;
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
SBThread::~SBThread()
{
}

bool
SBThread::IsValid() const
{
    return !m_opaque_wp.expired();
}

void
SBThread::Clear ()
{
    m_opaque_wp.reset();
}


StopReason
SBThread::GetStopReason()
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    StopReason reason = eStopReasonInvalid;
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        StopInfoSP stop_info_sp = exe_ctx.GetThreadPtr()->GetStopInfo ();
        if (stop_info_sp)
            reason =  stop_info_sp->GetStopReason();
    }

    if (log)
        log->Printf ("SBThread(%p)::GetStopReason () => %s", exe_ctx.GetThreadPtr(), 
                     Thread::StopReasonAsCString (reason));

    return reason;
}

size_t
SBThread::GetStopReasonDataCount ()
{
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        StopInfoSP stop_info_sp = exe_ctx.GetThreadPtr()->GetStopInfo ();
        if (stop_info_sp)
        {
            StopReason reason = stop_info_sp->GetStopReason();
            switch (reason)
            {
            case eStopReasonInvalid:
            case eStopReasonNone:
            case eStopReasonTrace:
            case eStopReasonPlanComplete:
                // There is no data for these stop reasons.
                return 0;

            case eStopReasonBreakpoint:
                {
                    break_id_t site_id = stop_info_sp->GetValue();
                    lldb::BreakpointSiteSP bp_site_sp (exe_ctx.GetProcessPtr()->GetBreakpointSiteList().FindByID (site_id));
                    if (bp_site_sp)
                        return bp_site_sp->GetNumberOfOwners () * 2;
                    else
                        return 0; // Breakpoint must have cleared itself...
                }
                break;

            case eStopReasonWatchpoint:
                return 1;

            case eStopReasonSignal:
                return 1;

            case eStopReasonException:
                return 1;
            }
        }
    }
    return 0;
}

uint64_t
SBThread::GetStopReasonDataAtIndex (uint32_t idx)
{
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        Thread *thread = exe_ctx.GetThreadPtr();
        StopInfoSP stop_info_sp = thread->GetStopInfo ();
        if (stop_info_sp)
        {
            StopReason reason = stop_info_sp->GetStopReason();
            switch (reason)
            {
            case eStopReasonInvalid:
            case eStopReasonNone:
            case eStopReasonTrace:
            case eStopReasonPlanComplete:
                // There is no data for these stop reasons.
                return 0;

            case eStopReasonBreakpoint:
                {
                    break_id_t site_id = stop_info_sp->GetValue();
                    lldb::BreakpointSiteSP bp_site_sp (exe_ctx.GetProcessPtr()->GetBreakpointSiteList().FindByID (site_id));
                    if (bp_site_sp)
                    {
                        uint32_t bp_index = idx / 2;
                        BreakpointLocationSP bp_loc_sp (bp_site_sp->GetOwnerAtIndex (bp_index));
                        if (bp_loc_sp)
                        {
                            if (bp_index & 1)
                            {
                                // Odd idx, return the breakpoint location ID
                                return bp_loc_sp->GetID();
                            }
                            else
                            {
                                // Even idx, return the breakpoint ID
                                return bp_loc_sp->GetBreakpoint().GetID();
                            }
                        }
                    }
                    return LLDB_INVALID_BREAK_ID;
                }
                break;

            case eStopReasonWatchpoint:
                return stop_info_sp->GetValue();

            case eStopReasonSignal:
                return stop_info_sp->GetValue();

            case eStopReasonException:
                return stop_info_sp->GetValue();
            }
        }
    }
    return 0;
}

size_t
SBThread::GetStopDescription (char *dst, size_t dst_len)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        StopInfoSP stop_info_sp = exe_ctx.GetThreadPtr()->GetStopInfo ();
        if (stop_info_sp)
        {
            const char *stop_desc = stop_info_sp->GetDescription();
            if (stop_desc)
            {
                if (log)
                    log->Printf ("SBThread(%p)::GetStopDescription (dst, dst_len) => \"%s\"", 
                                 exe_ctx.GetThreadPtr(), stop_desc);
                if (dst)
                    return ::snprintf (dst, dst_len, "%s", stop_desc);
                else
                {
                    // NULL dst passed in, return the length needed to contain the description
                    return ::strlen (stop_desc) + 1; // Include the NULL byte for size
                }
            }
            else
            {
                size_t stop_desc_len = 0;
                switch (stop_info_sp->GetStopReason())
                {
                case eStopReasonTrace:
                case eStopReasonPlanComplete:
                    {
                        static char trace_desc[] = "step";
                        stop_desc = trace_desc;
                        stop_desc_len = sizeof(trace_desc); // Include the NULL byte for size
                    }
                    break;

                case eStopReasonBreakpoint:
                    {
                        static char bp_desc[] = "breakpoint hit";
                        stop_desc = bp_desc;
                        stop_desc_len = sizeof(bp_desc); // Include the NULL byte for size
                    }
                    break;

                case eStopReasonWatchpoint:
                    {
                        static char wp_desc[] = "watchpoint hit";
                        stop_desc = wp_desc;
                        stop_desc_len = sizeof(wp_desc); // Include the NULL byte for size
                    }
                    break;

                case eStopReasonSignal:
                    {
                        stop_desc = exe_ctx.GetProcessPtr()->GetUnixSignals ().GetSignalAsCString (stop_info_sp->GetValue());
                        if (stop_desc == NULL || stop_desc[0] == '\0')
                        {
                            static char signal_desc[] = "signal";
                            stop_desc = signal_desc;
                            stop_desc_len = sizeof(signal_desc); // Include the NULL byte for size
                        }
                    }
                    break;

                case eStopReasonException:
                    {
                        char exc_desc[] = "exception";
                        stop_desc = exc_desc;
                        stop_desc_len = sizeof(exc_desc); // Include the NULL byte for size
                    }
                    break;          

                default:
                    break;
                }
                
                if (stop_desc && stop_desc[0])
                {
                    if (log)
                        log->Printf ("SBThread(%p)::GetStopDescription (dst, dst_len) => '%s'", 
                                     exe_ctx.GetThreadPtr(), stop_desc);

                    if (dst)
                        return ::snprintf (dst, dst_len, "%s", stop_desc) + 1; // Include the NULL byte

                    if (stop_desc_len == 0)
                        stop_desc_len = ::strlen (stop_desc) + 1; // Include the NULL byte
                        
                    return stop_desc_len;
                }
            }
        }
    }
    if (dst)
        *dst = 0;
    return 0;
}

SBValue
SBThread::GetStopReturnValue ()
{
    ValueObjectSP return_valobj_sp;
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        StopInfoSP stop_info_sp = exe_ctx.GetThreadPtr()->GetStopInfo ();
        if (stop_info_sp)
        {
            return_valobj_sp = StopInfo::GetReturnValueObject (stop_info_sp);
        }
    }
    
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));
    if (log)
        log->Printf ("SBThread(%p)::GetStopReturnValue () => %s", exe_ctx.GetThreadPtr(), 
                                                                  return_valobj_sp.get() 
                                                                      ? return_valobj_sp->GetValueAsCString() 
                                                                        : "<no return value>");
        
    return SBValue (return_valobj_sp);
}

void
SBThread::SetThread (const ThreadSP& lldb_object_sp)
{
    m_opaque_wp = lldb_object_sp;
}


lldb::tid_t
SBThread::GetThreadID () const
{
    ThreadSP thread_sp(m_opaque_wp.lock());
    if (thread_sp)
        return thread_sp->GetID();
    return LLDB_INVALID_THREAD_ID;
}

uint32_t
SBThread::GetIndexID () const
{
    ThreadSP thread_sp(m_opaque_wp.lock());
    if (thread_sp)
        return thread_sp->GetIndexID();
    return LLDB_INVALID_INDEX32;
}

const char *
SBThread::GetName () const
{
    const char *name = NULL;
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        name = exe_ctx.GetThreadPtr()->GetName();
    }
    
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));
    if (log)
        log->Printf ("SBThread(%p)::GetName () => %s", exe_ctx.GetThreadPtr(), name ? name : "NULL");

    return name;
}

const char *
SBThread::GetQueueName () const
{
    const char *name = NULL;
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        name = exe_ctx.GetThreadPtr()->GetQueueName();
    }
    
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));
    if (log)
        log->Printf ("SBThread(%p)::GetQueueName () => %s", exe_ctx.GetThreadPtr(), name ? name : "NULL");

    return name;
}


void
SBThread::StepOver (lldb::RunMode stop_other_threads)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    ExecutionContext exe_ctx (m_opaque_wp);

    if (log)
        log->Printf ("SBThread(%p)::StepOver (stop_other_threads='%s')", exe_ctx.GetThreadPtr(), 
                     Thread::RunModeAsCString (stop_other_threads));
    
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        Thread *thread = exe_ctx.GetThreadPtr();
        bool abort_other_plans = true;
        StackFrameSP frame_sp(thread->GetStackFrameAtIndex (0));

        if (frame_sp)
        {
            if (frame_sp->HasDebugInformation ())
            {
                SymbolContext sc(frame_sp->GetSymbolContext(eSymbolContextEverything));
                thread->QueueThreadPlanForStepRange (abort_other_plans, 
                                                     eStepTypeOver,
                                                     sc.line_entry.range, 
                                                     sc,
                                                     stop_other_threads,
                                                     false);
                
            }
            else
            {
                thread->QueueThreadPlanForStepSingleInstruction (true, 
                                                                 abort_other_plans, 
                                                                 stop_other_threads);
            }
        }

        Process *process = exe_ctx.GetProcessPtr();
        // Why do we need to set the current thread by ID here???
        process->GetThreadList().SetSelectedThreadByID (thread->GetID());
        Error error (process->Resume());
        if (error.Success())
        {
            // If we are doing synchronous mode, then wait for the
            // process to stop yet again!
            if (process->GetTarget().GetDebugger().GetAsyncExecution () == false)
                process->WaitForProcessToStop (NULL);
        }
    }
}

void
SBThread::StepInto (lldb::RunMode stop_other_threads)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    ExecutionContext exe_ctx (m_opaque_wp);

    if (log)
        log->Printf ("SBThread(%p)::StepInto (stop_other_threads='%s')", exe_ctx.GetThreadPtr(),
                     Thread::RunModeAsCString (stop_other_threads));
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        bool abort_other_plans = true;

        Thread *thread = exe_ctx.GetThreadPtr();
        StackFrameSP frame_sp(thread->GetStackFrameAtIndex (0));

        if (frame_sp && frame_sp->HasDebugInformation ())
        {
            bool avoid_code_without_debug_info = true;
            SymbolContext sc(frame_sp->GetSymbolContext(eSymbolContextEverything));
            thread->QueueThreadPlanForStepRange (abort_other_plans, 
                                                 eStepTypeInto, 
                                                 sc.line_entry.range, 
                                                 sc, 
                                                 stop_other_threads,
                                                 avoid_code_without_debug_info);
        }
        else
        {
            thread->QueueThreadPlanForStepSingleInstruction (false, 
                                                             abort_other_plans, 
                                                             stop_other_threads);
        }

        Process *process = exe_ctx.GetProcessPtr();
        // Why do we need to set the current thread by ID here???
        process->GetThreadList().SetSelectedThreadByID (thread->GetID());
        Error error (process->Resume());
        if (error.Success())
        {
            // If we are doing synchronous mode, then wait for the
            // process to stop yet again!
            if (process->GetTarget().GetDebugger().GetAsyncExecution () == false)
                process->WaitForProcessToStop (NULL);
        }
    }
}

void
SBThread::StepOut ()
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    ExecutionContext exe_ctx (m_opaque_wp);

    if (log)
        log->Printf ("SBThread(%p)::StepOut ()", exe_ctx.GetThreadPtr());
    
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        bool abort_other_plans = true;
        bool stop_other_threads = true;

        Thread *thread = exe_ctx.GetThreadPtr();

        thread->QueueThreadPlanForStepOut (abort_other_plans, 
                                              NULL, 
                                              false, 
                                              stop_other_threads, 
                                              eVoteYes, 
                                              eVoteNoOpinion,
                                              0);
        
        Process *process = exe_ctx.GetProcessPtr();
        process->GetThreadList().SetSelectedThreadByID (thread->GetID());
        Error error (process->Resume());
        if (error.Success())
        {
            // If we are doing synchronous mode, then wait for the
            // process to stop yet again!
            if (process->GetTarget().GetDebugger().GetAsyncExecution () == false)
                process->WaitForProcessToStop (NULL);
        }
    }
}

void
SBThread::StepOutOfFrame (lldb::SBFrame &sb_frame)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    ExecutionContext exe_ctx (m_opaque_wp);
    StackFrameSP frame_sp (sb_frame.GetFrameSP());
    if (log)
    {
        SBStream frame_desc_strm;
        sb_frame.GetDescription (frame_desc_strm);
        log->Printf ("SBThread(%p)::StepOutOfFrame (frame = SBFrame(%p): %s)", exe_ctx.GetThreadPtr(), frame_sp.get(), frame_desc_strm.GetData());
    }

    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        bool abort_other_plans = true;
        bool stop_other_threads = true;
        Thread *thread = exe_ctx.GetThreadPtr();

        thread->QueueThreadPlanForStepOut (abort_other_plans, 
                                              NULL, 
                                              false, 
                                              stop_other_threads, 
                                              eVoteYes, 
                                              eVoteNoOpinion,
                                              frame_sp->GetFrameIndex());
        
        Process *process = exe_ctx.GetProcessPtr();
        process->GetThreadList().SetSelectedThreadByID (thread->GetID());
        Error error (process->Resume());
        if (error.Success())
        {
            // If we are doing synchronous mode, then wait for the
            // process to stop yet again!
            if (process->GetTarget().GetDebugger().GetAsyncExecution () == false)
                process->WaitForProcessToStop (NULL);
        }
    }
}

void
SBThread::StepInstruction (bool step_over)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    ExecutionContext exe_ctx (m_opaque_wp);


    if (log)
        log->Printf ("SBThread(%p)::StepInstruction (step_over=%i)", exe_ctx.GetThreadPtr(), step_over);
    
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        Thread *thread = exe_ctx.GetThreadPtr();
        Process *process = exe_ctx.GetProcessPtr();
        thread->QueueThreadPlanForStepSingleInstruction (step_over, true, true);
        process->GetThreadList().SetSelectedThreadByID (thread->GetID());
        Error error (process->Resume());
        if (error.Success())
        {
            // If we are doing synchronous mode, then wait for the
            // process to stop yet again!
            if (process->GetTarget().GetDebugger().GetAsyncExecution () == false)
                process->WaitForProcessToStop (NULL);
        }
    }
}

void
SBThread::RunToAddress (lldb::addr_t addr)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    ExecutionContext exe_ctx (m_opaque_wp);

    if (log)
        log->Printf ("SBThread(%p)::RunToAddress (addr=0x%llx)", exe_ctx.GetThreadPtr(), addr);
    
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        bool abort_other_plans = true;
        bool stop_other_threads = true;

        Address target_addr (addr);

        Thread *thread = exe_ctx.GetThreadPtr();
        Process *process = exe_ctx.GetProcessPtr();

        thread->QueueThreadPlanForRunToAddress (abort_other_plans, target_addr, stop_other_threads);
        process->GetThreadList().SetSelectedThreadByID (thread->GetID());
        Error error (process->Resume());
        if (error.Success())
        {
            // If we are doing synchronous mode, then wait for the
            // process to stop yet again!
            if (process->GetTarget().GetDebugger().GetAsyncExecution () == false)
                process->WaitForProcessToStop (NULL);
        }
    }
}

SBError
SBThread::StepOverUntil (lldb::SBFrame &sb_frame, 
                         lldb::SBFileSpec &sb_file_spec, 
                         uint32_t line)
{
    SBError sb_error;
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));
    char path[PATH_MAX];
    
    ExecutionContext exe_ctx (m_opaque_wp);    
    StackFrameSP frame_sp (sb_frame.GetFrameSP());

    if (log)
    {
        SBStream frame_desc_strm;
        sb_frame.GetDescription (frame_desc_strm);
        sb_file_spec->GetPath (path, sizeof(path));
        log->Printf ("SBThread(%p)::StepOverUntil (frame = SBFrame(%p): %s, file+line = %s:%u)", 
                     exe_ctx.GetThreadPtr(), 
                     frame_sp.get(), 
                     frame_desc_strm.GetData(),
                     path, line);
    }

    if (exe_ctx.HasThreadScope())
    {
        Target *target = exe_ctx.GetTargetPtr();
        Mutex::Locker api_locker (target->GetAPIMutex());
        Thread *thread = exe_ctx.GetThreadPtr();

        if (line == 0)
        {
            sb_error.SetErrorString("invalid line argument");
            return sb_error;
        }
        
        StackFrameSP frame_sp;
        if (!frame_sp)
        {
            frame_sp = thread->GetSelectedFrame ();
            if (!frame_sp)
                frame_sp = thread->GetStackFrameAtIndex (0);
        }
    
        SymbolContext frame_sc;
        if (!frame_sp)        
        {
            sb_error.SetErrorString("no valid frames in thread to step");
            return sb_error;
        }

        // If we have a frame, get its line
        frame_sc = frame_sp->GetSymbolContext (eSymbolContextCompUnit  |
                                               eSymbolContextFunction  | 
                                               eSymbolContextLineEntry | 
                                               eSymbolContextSymbol    );
                                               
        if (frame_sc.comp_unit == NULL)
        {
            sb_error.SetErrorStringWithFormat("frame %u doesn't have debug information", frame_sp->GetFrameIndex());
            return sb_error;
        }
        
        FileSpec step_file_spec;
        if (sb_file_spec.IsValid())
        {
            // The file spec passed in was valid, so use it
            step_file_spec = sb_file_spec.ref();
        }
        else
        {
            if (frame_sc.line_entry.IsValid())
                step_file_spec = frame_sc.line_entry.file;
            else
            {
                sb_error.SetErrorString("invalid file argument or no file for frame");
                return sb_error;
            }
        }
    
        // Grab the current function, then we will make sure the "until" address is
        // within the function.  We discard addresses that are out of the current
        // function, and then if there are no addresses remaining, give an appropriate
        // error message.
        
        bool all_in_function = true;
        AddressRange fun_range = frame_sc.function->GetAddressRange();
        
        std::vector<addr_t> step_over_until_addrs;
        const bool abort_other_plans = true;
        const bool stop_other_threads = true;
        const bool check_inlines = true;
        const bool exact = false;

        SymbolContextList sc_list;
        const uint32_t num_matches = frame_sc.comp_unit->ResolveSymbolContext (step_file_spec, 
                                                                               line, 
                                                                               check_inlines, 
                                                                               exact, 
                                                                               eSymbolContextLineEntry, 
                                                                               sc_list);
        if (num_matches > 0)
        {
            SymbolContext sc;
            for (uint32_t i=0; i<num_matches; ++i)
            {
                if (sc_list.GetContextAtIndex(i, sc))
                {
                    addr_t step_addr = sc.line_entry.range.GetBaseAddress().GetLoadAddress(target);
                    if (step_addr != LLDB_INVALID_ADDRESS)
                    {
                        if (fun_range.ContainsLoadAddress(step_addr, target))
                            step_over_until_addrs.push_back(step_addr);
                        else
                            all_in_function = false;
                    }
                }
            }
        }
        
        if (step_over_until_addrs.empty())
        {
            if (all_in_function)
            {
                step_file_spec.GetPath (path, sizeof(path));
                sb_error.SetErrorStringWithFormat("No line entries for %s:%u", path, line);
            }
            else
                sb_error.SetErrorString ("step until target not in current function");
        }
        else
        {
            thread->QueueThreadPlanForStepUntil (abort_other_plans, 
                                                 &step_over_until_addrs[0],
                                                 step_over_until_addrs.size(),
                                                 stop_other_threads,
                                                 frame_sp->GetFrameIndex());      

            Process *process = exe_ctx.GetProcessPtr();

            process->GetThreadList().SetSelectedThreadByID (thread->GetID());
            sb_error.ref() = process->Resume();
            if (sb_error->Success())
            {
                // If we are doing synchronous mode, then wait for the
                // process to stop yet again!
                if (process->GetTarget().GetDebugger().GetAsyncExecution () == false)
                    process->WaitForProcessToStop (NULL);
            }
        }
    }
    else
    {
        sb_error.SetErrorString("this SBThread object is invalid");
    }
    return sb_error;
}


bool
SBThread::Suspend()
{
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        exe_ctx.GetThreadPtr()->SetResumeState (eStateSuspended);
        return true;
    }
    return false;
}

bool
SBThread::Resume ()
{
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        exe_ctx.GetThreadPtr()->SetResumeState (eStateRunning);
        return true;
    }
    return false;
}

bool
SBThread::IsSuspended()
{
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
        return exe_ctx.GetThreadPtr()->GetResumeState () == eStateSuspended;
    return false;
}

SBProcess
SBThread::GetProcess ()
{

    SBProcess sb_process;
    ProcessSP process_sp;
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        // Have to go up to the target so we can get a shared pointer to our process...
        sb_process.SetSP (exe_ctx.GetProcessSP());
    }

    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));
    if (log)
    {
        SBStream frame_desc_strm;
        sb_process.GetDescription (frame_desc_strm);
        log->Printf ("SBThread(%p)::GetProcess () => SBProcess(%p): %s", exe_ctx.GetThreadPtr(),
                     process_sp.get(), frame_desc_strm.GetData());
    }

    return sb_process;
}

uint32_t
SBThread::GetNumFrames ()
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    uint32_t num_frames = 0;
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        num_frames = exe_ctx.GetThreadPtr()->GetStackFrameCount();
    }

    if (log)
        log->Printf ("SBThread(%p)::GetNumFrames () => %u", exe_ctx.GetThreadPtr(), num_frames);

    return num_frames;
}

SBFrame
SBThread::GetFrameAtIndex (uint32_t idx)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    SBFrame sb_frame;
    StackFrameSP frame_sp;
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        frame_sp = exe_ctx.GetThreadPtr()->GetStackFrameAtIndex (idx);
        sb_frame.SetFrameSP (frame_sp);
    }

    if (log)
    {
        SBStream frame_desc_strm;
        sb_frame.GetDescription (frame_desc_strm);
        log->Printf ("SBThread(%p)::GetFrameAtIndex (idx=%d) => SBFrame(%p): %s", 
                     exe_ctx.GetThreadPtr(), idx, frame_sp.get(), frame_desc_strm.GetData());
    }

    return sb_frame;
}

lldb::SBFrame
SBThread::GetSelectedFrame ()
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    SBFrame sb_frame;
    StackFrameSP frame_sp;
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        frame_sp = exe_ctx.GetThreadPtr()->GetSelectedFrame ();
        sb_frame.SetFrameSP (frame_sp);
    }

    if (log)
    {
        SBStream frame_desc_strm;
        sb_frame.GetDescription (frame_desc_strm);
        log->Printf ("SBThread(%p)::GetSelectedFrame () => SBFrame(%p): %s", 
                     exe_ctx.GetThreadPtr(), frame_sp.get(), frame_desc_strm.GetData());
    }

    return sb_frame;
}

lldb::SBFrame
SBThread::SetSelectedFrame (uint32_t idx)
{
    LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_API));

    SBFrame sb_frame;
    StackFrameSP frame_sp;
    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        Mutex::Locker api_locker (exe_ctx.GetTargetPtr()->GetAPIMutex());
        Thread *thread = exe_ctx.GetThreadPtr();
        frame_sp = thread->GetStackFrameAtIndex (idx);
        if (frame_sp)
        {
            thread->SetSelectedFrame (frame_sp.get());
            sb_frame.SetFrameSP (frame_sp);
        }
    }

    if (log)
    {
        SBStream frame_desc_strm;
        sb_frame.GetDescription (frame_desc_strm);
        log->Printf ("SBThread(%p)::SetSelectedFrame (idx=%u) => SBFrame(%p): %s", 
                     exe_ctx.GetThreadPtr(), idx, frame_sp.get(), frame_desc_strm.GetData());
    }
    return sb_frame;
}


bool
SBThread::operator == (const SBThread &rhs) const
{
    return m_opaque_wp.lock().get() == rhs.m_opaque_wp.lock().get();
}

bool
SBThread::operator != (const SBThread &rhs) const
{
    return m_opaque_wp.lock().get() != rhs.m_opaque_wp.lock().get();
}

bool
SBThread::GetDescription (SBStream &description) const
{
    Stream &strm = description.ref();

    ExecutionContext exe_ctx (m_opaque_wp);
    if (exe_ctx.HasThreadScope())
    {
        strm.Printf("SBThread: tid = 0x%4.4llx", exe_ctx.GetThreadPtr()->GetID());
    }
    else
        strm.PutCString ("No value");
    
    return true;
}
