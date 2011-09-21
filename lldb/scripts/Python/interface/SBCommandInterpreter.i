//===-- SWIG Interface for SBCommandInterpreter -----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

namespace lldb {

%feature("docstring",
"SBCommandInterpreter handles/interprets commands for lldb.  You get the
command interpreter from the SBDebugger instance. For example (from test/
python_api/interpreter/TestCommandInterpreterAPI.py),

    def command_interpreter_api(self):
        '''Test the SBCommandInterpreter APIs.'''
        exe = os.path.join(os.getcwd(), 'a.out')

        # Create a target by the debugger.
        target = self.dbg.CreateTarget(exe)
        self.assertTrue(target, VALID_TARGET)

        # Retrieve the associated command interpreter from our debugger.
        ci = self.dbg.GetCommandInterpreter()
        self.assertTrue(ci, VALID_COMMAND_INTERPRETER)

        # Exercise some APIs....

        self.assertTrue(ci.HasCommands())
        self.assertTrue(ci.HasAliases())
        self.assertTrue(ci.HasAliasOptions())
        self.assertTrue(ci.CommandExists('breakpoint'))
        self.assertTrue(ci.CommandExists('target'))
        self.assertTrue(ci.CommandExists('platform'))
        self.assertTrue(ci.AliasExists('file'))
        self.assertTrue(ci.AliasExists('run'))
        self.assertTrue(ci.AliasExists('bt'))

        res = lldb.SBCommandReturnObject()
        ci.HandleCommand('breakpoint set -f main.c -l %d' % self.line, res)
        self.assertTrue(res.Succeeded())
        ci.HandleCommand('process launch', res)
        self.assertTrue(res.Succeeded())

        process = ci.GetProcess()
        self.assertTrue(process)

        ...

The HandleCommand() instance method takes two args: the command string and
an SBCommandReturnObject instance which encapsulates the result of command
execution.
") SBCommandInterpreter;
class SBCommandInterpreter
{
public:
    enum
    {
        eBroadcastBitThreadShouldExit       = (1 << 0),
        eBroadcastBitResetPrompt            = (1 << 1),
        eBroadcastBitQuitCommandReceived    = (1 << 2),           // User entered quit 
        eBroadcastBitAsynchronousOutputData = (1 << 3),
        eBroadcastBitAsynchronousErrorData  = (1 << 4)
    };

    SBCommandInterpreter (const lldb::SBCommandInterpreter &rhs);
    
    ~SBCommandInterpreter ();

    static const char * 
    GetArgumentTypeAsCString (const lldb::CommandArgumentType arg_type);
    
    static const char *
    GetArgumentDescriptionAsCString (const lldb::CommandArgumentType arg_type);
    
    bool
    IsValid() const;

    bool
    CommandExists (const char *cmd);

    bool
    AliasExists (const char *cmd);

    lldb::SBBroadcaster
    GetBroadcaster ();

    bool
    HasCommands ();

    bool
    HasAliases ();

    bool
    HasAliasOptions ();

    lldb::SBProcess
    GetProcess ();

    ssize_t
    WriteToScriptInterpreter (const char *src);

    ssize_t
    WriteToScriptInterpreter (const char *src, size_t src_len);

    void
    SourceInitFileInHomeDirectory (lldb::SBCommandReturnObject &result);

    void
    SourceInitFileInCurrentWorkingDirectory (lldb::SBCommandReturnObject &result);

    lldb::ReturnStatus
    HandleCommand (const char *command_line, lldb::SBCommandReturnObject &result, bool add_to_history = false);

    int
    HandleCompletion (const char *current_line,
                      uint32_t cursor_pos,
                      int match_start_point,
                      int max_return_elements,
                      lldb::SBStringList &matches);
};

} // namespace lldb
