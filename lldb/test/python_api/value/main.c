//===-- main.c --------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include <stdio.h>

// This simple program is to test the lldb Python API SBValue.GetChildAtIndex().

int g_my_int = 100;

const char *days_of_week[7] = { "Sunday",
                                "Monday",
                                "Tuesday",
                                "Wednesday",
                                "Thursday",
                                "Friday",
                                "Saturday" };

int main (int argc, char const *argv[])
{
    int i;
    const char **str_ptr = days_of_week;
    for (i = 0; i < 7; ++i)
        printf("%s\n", str_ptr[i]); // Break at this line
                                    // and do str_ptr_val.GetChildAtIndex(5, lldb.eNoDynamicValues, True).
    
    return 0;
}
