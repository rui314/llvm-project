# REQUIRES: x86

# RUN: llvm-mc -filetype=obj -triple=x86_64-unknown-linux %s -o %t
# RUN: ld.lld %t -o %t2
# RUN: llvm-readobj -symbols %t2 | FileCheck %s
# RUN: ld.lld %t -o %t2 -no-call-graph-profile-sort
# RUN: llvm-readobj -symbols %t2 | FileCheck %s --check-prefix=NOSORT

    .section    .text.hot._Z4fooav,"ax",@progbits
    .globl  _Z4fooav
_Z4fooav:
    retq

    .section    .text.hot._Z4foobv,"ax",@progbits
    .globl  _Z4foobv
_Z4foobv:
    retq

    .section    .text.hot._Z3foov,"ax",@progbits
    .globl  _Z3foov
_Z3foov:
    retq

    .section    .text.hot._start,"ax",@progbits
    .globl  _start
_start:
    retq


    .cg_profile _start, _Z3foov, 1
    .cg_profile _Z4fooav, _Z4foobv, 1
    .cg_profile _Z3foov, _Z4fooav, 1

# CHECK:          Name: _Z3foov
# CHECK-NEXT:     Value: 0x201001
# CHECK:          Name: _Z4fooav
# CHECK-NEXT:     Value: 0x201002
# CHECK:          Name: _Z4foobv
# CHECK-NEXT:     Value: 0x201003
# CHECK:          Name: _start
# CHECK-NEXT:     Value: 0x201000
	
# NOSORT:          Name: _Z3foov
# NOSORT-NEXT:     Value: 0x201002
# NOSORT:          Name: _Z4fooav
# NOSORT-NEXT:     Value: 0x201000
# NOSORT:          Name: _Z4foobv
# NOSORT-NEXT:     Value: 0x201001
# NOSORT:          Name: _start
# NOSORT-NEXT:     Value: 0x201003
