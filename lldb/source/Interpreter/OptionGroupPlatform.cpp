//===-- OptionGroupPlatform.cpp ---------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Interpreter/OptionGroupPlatform.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Target/Platform.h"

using namespace lldb;
using namespace lldb_private;

PlatformSP 
OptionGroupPlatform::CreatePlatformWithOptions (CommandInterpreter &interpreter, bool make_selected, Error& error)
{
    PlatformSP platform_sp;
    if (!m_platform_name.empty())
    {
        platform_sp = Platform::Create (m_platform_name.c_str(), error);
        
        if (platform_sp)
        {
            interpreter.GetDebugger().GetPlatformList().Append (platform_sp, make_selected);
            if (m_os_version_major != UINT32_MAX)
            {
                platform_sp->SetOSVersion (m_os_version_major,
                                           m_os_version_minor,
                                           m_os_version_update);
            }
            
            if (m_sdk_sysroot)
                platform_sp->SetSDKRootDirectory (m_sdk_sysroot);

            if (m_sdk_build)
                platform_sp->SetSDKBuild (m_sdk_build);
        }
    }
    return platform_sp;
}

void
OptionGroupPlatform::OptionParsingStarting (CommandInterpreter &interpreter)
{
    m_platform_name.clear();
    m_sdk_sysroot.Clear();
    m_sdk_build.Clear();
    m_os_version_major = UINT32_MAX;
    m_os_version_minor = UINT32_MAX;
    m_os_version_update = UINT32_MAX;
}

static OptionDefinition
g_option_table[] =
{
    { LLDB_OPT_SET_ALL, false, "platform", 'p', required_argument, NULL, 0, eArgTypePlatform, "Specify name of the platform to use for this target, creating the platform if necessary."},
    { LLDB_OPT_SET_ALL, false, "version" , 'v', required_argument, NULL, 0, eArgTypeNone, "Specify the initial SDK version to use prior to connecting." },
    { LLDB_OPT_SET_ALL, false, "build"   , 'b', required_argument, NULL, 0, eArgTypeNone, "Specify the initial SDK build number." },
    { LLDB_OPT_SET_ALL, false, "sysroot" , 's', required_argument, NULL, 0, eArgTypeFilename, "Specify the SDK root directory that contains a root of all remote system files." }
};

static const uint32_t k_option_table_size = sizeof(g_option_table)/sizeof (OptionDefinition);

const OptionDefinition*
OptionGroupPlatform::GetDefinitions ()
{
    if (m_include_platform_option)
        return g_option_table;
    return g_option_table + 1;
}

uint32_t
OptionGroupPlatform::GetNumDefinitions ()
{
    if (m_include_platform_option)
        return k_option_table_size;
    return k_option_table_size - 1;
}


Error
OptionGroupPlatform::SetOptionValue (CommandInterpreter &interpreter,
                                     uint32_t option_idx,
                                     const char *option_arg)
{
    Error error;
    if (!m_include_platform_option)
        ++option_idx;
    
    char short_option = (char) g_option_table[option_idx].short_option;
    
    switch (short_option)
    {
        case 'p':
            m_platform_name.assign (option_arg);
            break;
            
        case 'v':
            if (Args::StringToVersion (option_arg, 
                                       m_os_version_major, 
                                       m_os_version_minor, 
                                       m_os_version_update) == option_arg)
                error.SetErrorStringWithFormat ("invalid version string '%s'", option_arg);
            break;
            
        case 'b':
            m_sdk_build.SetCString (option_arg);
            break;
            
        case 's':
            m_sdk_sysroot.SetCString (option_arg);
            break;

        default:
            error.SetErrorStringWithFormat ("Unrecognized option '%c'.\n", short_option);
            break;
    }
    return error;
}
