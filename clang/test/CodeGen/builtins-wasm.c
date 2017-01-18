// RUN: %clang_cc1 -triple wasm32-unknown-unknown -O3 -emit-llvm -o - %s \
// RUN:   | FileCheck %s -check-prefix=WEBASSEMBLY32
// RUN: %clang_cc1 -triple wasm64-unknown-unknown -O3 -emit-llvm -o - %s \
// RUN:   | FileCheck %s -check-prefix=WEBASSEMBLY64

__SIZE_TYPE__ f1(void) {
  return __builtin_wasm_current_memory();
// WEBASSEMBLY32: call {{i.*}} @llvm.wasm.current.memory.i32()
// WEBASSEMBLY64: call {{i.*}} @llvm.wasm.current.memory.i64()
}

__SIZE_TYPE__ f2(__SIZE_TYPE__ delta) {
  return __builtin_wasm_grow_memory(delta);
// WEBASSEMBLY32: call i32 @llvm.wasm.grow.memory.i32(i32 %{{.*}})
// WEBASSEMBLY64: call i64 @llvm.wasm.grow.memory.i64(i64 %{{.*}})
}
