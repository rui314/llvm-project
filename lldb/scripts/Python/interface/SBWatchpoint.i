//===-- SWIG Interface for SBWatchpoint -----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

namespace lldb {

%feature("docstring",
"Represents an instance of watchpoint for a specific target program.

A watchpoint is determined by the address and the byte size that resulted in
this particular instantiation.  Each watchpoint has its settable options.

See also SBTarget.watchpoint_iter() for for example usage of iterating through
the watchpoints of the target."
) SBWatchpoint;
class SBWatchpoint
{
public:

    SBWatchpoint ();

    SBWatchpoint (const lldb::SBWatchpoint &rhs);

    ~SBWatchpoint ();

    bool
    IsValid();

    SBError
    GetError();

    watch_id_t
    GetID ();

    %feature("docstring", "
    //------------------------------------------------------------------
    /// With -1 representing an invalid hardware index.
    //------------------------------------------------------------------
    ") GetHardwareIndex;
    int32_t
    GetHardwareIndex ();

    lldb::addr_t
    GetWatchAddress ();

    size_t
    GetWatchSize();

    void
    SetEnabled(bool enabled);

    bool
    IsEnabled ();

    uint32_t
    GetHitCount ();

    uint32_t
    GetIgnoreCount ();

    void
    SetIgnoreCount (uint32_t n);

    const char *
    GetCondition ();

    void 
    SetCondition (const char *condition);
    
    bool
    GetDescription (lldb::SBStream &description, DescriptionLevel level);
};

} // namespace lldb
