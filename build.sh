#!/bin/bash
CMAKE=/ssd/cmake/bin/cmake

set -x
TOP=`pwd`

mkdir -p $TOP/build/cgprofile
cd $TOP/build/cgprofile

CC=`which clang` CXX=`which clang++` $CMAKE -GNinja -DCMAKE_INSTALL_PREFIX=$TOP/build/bin -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_LLD=true -DLLVM_ENABLE_PROJECTS='clang;compiler-rt' $TOP/llvm
ninja install

mkdir -p $TOP/build/instrumented
cd $TOP/build/instrumented

CC=$TOP/build/bin/bin/clang CXX=$TOP/build/bin/bin/clang++ $CMAKE -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C{,XX}_FLAGS=-fprofile-instr-generate -DLLVM_ENABLE_LLD=true -DLLVM_ENABLE_PROJECTS='clang;lld' $TOP/llvm

ninja check-lld
$TOP/build/cgprofile/bin/llvm-profdata merge tools/lld/test/ELF/default.profraw -o default.profdata

mkdir -p $TOP/build/optimized
cd $TOP/build/optimized

CC=$TOP/build/bin/bin/clang CXX=$TOP/build/bin/bin/clang++ $CMAKE -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C{,XX}_FLAGS=-fprofile-instr-use=$TOP/build/instrumented/default.profdata -DLLVM_ENABLE_LLD=true -DLLVM_ENABLE_PROJECTS='clang;lld' $TOP/llvm
ninja
