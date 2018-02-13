set(LLVM_TARGET_DEFINITIONS Options.td)
tablegen(LLVM Options.inc -gen-opt-parser-defs)
add_public_tablegen_target(MachOOptionsTableGen)

add_lld_library(lldMachO2
  Driver.cpp
  InputFiles.cpp
  InputSection.cpp
  OutputSegment.cpp
  SymbolTable.cpp
  Writer.cpp

  LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  BinaryFormat
  Core
  DebugInfoDWARF
  LTO
  MC
  Object
  Option
  Support

  LINK_LIBS
  lldCommon
  ${LLVM_PTHREAD_LIB}

  DEPENDS
  ELFOptionsTableGen
  ${tablegen_deps}
  )
