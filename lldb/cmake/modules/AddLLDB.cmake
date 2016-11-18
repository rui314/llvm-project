function(lldb_link_common_libs name targetkind)
  if (NOT LLDB_USED_LIBS)
    return()
  endif()

  if(${targetkind} MATCHES "SHARED")
    set(LINK_KEYWORD PRIVATE)
  endif()

  if(${targetkind} MATCHES "SHARED" OR ${targetkind} MATCHES "EXE")
    if (LLDB_LINKER_SUPPORTS_GROUPS)
      target_link_libraries(${name} ${LINK_KEYWORD}
                            -Wl,--start-group ${LLDB_USED_LIBS} -Wl,--end-group)
    else()
      target_link_libraries(${name} ${LINK_KEYWORD} ${LLDB_USED_LIBS})
    endif()
  endif()
endfunction(lldb_link_common_libs)

macro(add_lldb_library name)
  # only supported parameters to this macro are the optional
  # MODULE;SHARED;STATIC library type and source files
  cmake_parse_arguments(PARAM
    "MODULE;SHARED;STATIC;OBJECT"
    ""
    "DEPENDS"
    ${ARGN})
  llvm_process_sources(srcs ${PARAM_UNPARSED_ARGUMENTS})

  if (MSVC_IDE OR XCODE)
    string(REGEX MATCHALL "/[^/]+" split_path ${CMAKE_CURRENT_SOURCE_DIR})
    list(GET split_path -1 dir)
    file(GLOB_RECURSE headers
      ../../include/lldb${dir}/*.h)
    set(srcs ${srcs} ${headers})
  endif()
  if (PARAM_MODULE)
    set(libkind MODULE)
  elseif (PARAM_SHARED)
    set(libkind SHARED)
  elseif (PARAM_OBJECT)
    set(libkind OBJECT)
  else ()
    # PARAM_STATIC or library type unspecified. BUILD_SHARED_LIBS
    # does not control the kind of libraries created for LLDB,
    # only whether or not they link to shared/static LLVM/Clang
    # libraries.
    set(libkind STATIC)
  endif()

  #PIC not needed on Win
  if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
  endif()

  if (PARAM_OBJECT)
    add_library(${name} ${libkind} ${srcs})
  else()
    if (PARAM_SHARED)
      if (LLDB_LINKER_SUPPORTS_GROUPS)
        llvm_add_library(${name} ${libkind} ${srcs} LINK_LIBS
                                -Wl,--start-group ${LLDB_USED_LIBS} -Wl,--end-group
                                -Wl,--start-group ${CLANG_USED_LIBS} -Wl,--end-group
                                DEPENDS ${PARAM_DEPENDS}
          )
      else()
        llvm_add_library(${name} ${libkind} ${srcs} LINK_LIBS
                                ${LLDB_USED_LIBS} ${CLANG_USED_LIBS}
                                DEPENDS ${PARAM_DEPENDS}
          )
      endif()
    else()
      llvm_add_library(${name} ${libkind} ${srcs} DEPENDS ${PARAM_DEPENDS})
    endif()

    if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY OR ${name} STREQUAL "liblldb")
      if (PARAM_SHARED)
        set(out_dir lib${LLVM_LIBDIR_SUFFIX})
        if(${name} STREQUAL "liblldb" AND LLDB_BUILD_FRAMEWORK)
          set(out_dir ${LLDB_FRAMEWORK_INSTALL_DIR})
        endif()
        install(TARGETS ${name}
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION ${out_dir}
          ARCHIVE DESTINATION ${out_dir})
      else()
        install(TARGETS ${name}
          LIBRARY DESTINATION lib${LLVM_LIBDIR_SUFFIX}
          ARCHIVE DESTINATION lib${LLVM_LIBDIR_SUFFIX})
      endif()
    endif()
  endif()

  # Hack: only some LLDB libraries depend on the clang autogenerated headers,
  # but it is simple enough to make all of LLDB depend on some of those
  # headers without negatively impacting much of anything.
  get_property(CLANG_TABLEGEN_TARGETS GLOBAL PROPERTY CLANG_TABLEGEN_TARGETS)
  if(CLANG_TABLEGEN_TARGETS)
    add_dependencies(${name} ${CLANG_TABLEGEN_TARGETS})
  endif()

  set_target_properties(${name} PROPERTIES FOLDER "lldb libraries")
endmacro(add_lldb_library)

macro(add_lldb_executable name)
  cmake_parse_arguments(ARG "INCLUDE_IN_FRAMEWORK" "" "" ${ARGN})
  add_llvm_executable(${name} ${ARG_UNPARSED_ARGUMENTS})
  set_target_properties(${name} PROPERTIES
    FOLDER "lldb executables")

  if(LLDB_BUILD_FRAMEWORK)
    if(ARG_INCLUDE_IN_FRAMEWORK)
      string(REGEX REPLACE "[^/]+" ".." _dots ${LLDB_FRAMEWORK_INSTALL_DIR})
      set_target_properties(${name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:liblldb>/Resources
            BUILD_WITH_INSTALL_RPATH On
            INSTALL_RPATH "@loader_path/../../../../${_dots}/${LLDB_FRAMEWORK_INSTALL_DIR}")

      add_llvm_tool_symlink(${name} $<TARGET_FILE:${name}> ARG_ALWAYS_GENERATE)
    else()
      set_target_properties(${name} PROPERTIES
            BUILD_WITH_INSTALL_RPATH On
            INSTALL_RPATH "@loader_path/../${LLDB_FRAMEWORK_INSTALL_DIR}")
    endif()
  endif()
endmacro(add_lldb_executable)

# Support appending linker flags to an existing target.
# This will preserve the existing linker flags on the
# target, if there are any.
function(lldb_append_link_flags target_name new_link_flags)
  # Retrieve existing linker flags.
  get_target_property(current_link_flags ${target_name} LINK_FLAGS)

  # If we had any linker flags, include them first in the new linker flags.
  if(current_link_flags)
    set(new_link_flags "${current_link_flags} ${new_link_flags}")
  endif()

  # Now set them onto the target.
  set_target_properties(${target_name} PROPERTIES LINK_FLAGS ${new_link_flags})
endfunction()
