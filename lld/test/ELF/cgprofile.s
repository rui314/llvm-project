# REQUIRES: x86
#
# RUN: llvm-mc -filetype=obj -triple=x86_64-unknown-linux %s -o %t1
# RUN: ld.lld %t1 -e a -o %t -call-graph-profile-file %p/Inputs/cgprofile.txt
# RUN: llvm-readobj -symbols %t | FileCheck %s

    .section .text.a,"ax",@progbits
    .global a
a:
    .zero 20

    .section .text.b,"ax",@progbits
    .global b
b:
    .zero 1
    
    .section .text.c,"ax",@progbits
    .global c
c:
    .zero 4095
    
    .section .text.d,"ax",@progbits
    .global d
d:
    .zero 51
    
    .section .text.e,"ax",@progbits
    .global e
e:
    .zero 42

    .section .text.f,"ax",@progbits
    .global f
f:
    .zero 42
	
    .section .text.g,"ax",@progbits
    .global g
g:
	.zero 34
	
    .section .text.h,"ax",@progbits
    .global h
h:

# CHECK:     Symbols [
# CHECK-NEXT:  Symbol {
# CHECK-NEXT:    Name:  (0)
# CHECK-NEXT:    Value: 0x0
# CHECK-NEXT:    Size: 0
# CHECK-NEXT:    Binding: Local (0x0)
# CHECK-NEXT:    Type: None (0x0)
# CHECK-NEXT:    Other: 0
# CHECK-NEXT:    Section: Undefined (0x0)
# CHECK-NEXT:  }
# CHECK-NEXT:  Symbol {
# CHECK-NEXT:    Name: a
# CHECK-NEXT:    Value: 0x202022
# CHECK-NEXT:    Size: 0
# CHECK-NEXT:    Binding: Global (0x1)
# CHECK-NEXT:    Type: None (0x0)
# CHECK-NEXT:    Other: 0
# CHECK-NEXT:    Section: .text
# CHECK-NEXT:  }
# CHECK-NEXT:  Symbol {
# CHECK-NEXT:    Name: b
# CHECK-NEXT:    Value: 0x202021
# CHECK-NEXT:    Size: 0
# CHECK-NEXT:    Binding: Global (0x1)
# CHECK-NEXT:    Type: None (0x0)
# CHECK-NEXT:    Other: 0
# CHECK-NEXT:    Section: .text
# CHECK-NEXT:  }
# CHECK-NEXT:  Symbol {
# CHECK-NEXT:    Name: c
# CHECK-NEXT:    Value: 0x201022
# CHECK-NEXT:    Size: 0
# CHECK-NEXT:    Binding: Global (0x1)
# CHECK-NEXT:    Type: None (0x0)
# CHECK-NEXT:    Other: 0
# CHECK-NEXT:    Section: .text
# CHECK-NEXT:  }
# CHECK-NEXT:  Symbol {
# CHECK-NEXT:    Name: d
# CHECK-NEXT:    Value: 0x20208A
# CHECK-NEXT:    Size: 0
# CHECK-NEXT:    Binding: Global (0x1)
# CHECK-NEXT:    Type: None (0x0)
# CHECK-NEXT:    Other: 0
# CHECK-NEXT:    Section: .text
# CHECK-NEXT:  }
# CHECK-NEXT:  Symbol {
# CHECK-NEXT:    Name: e
# CHECK-NEXT:    Value: 0x202060
# CHECK-NEXT:    Size: 0
# CHECK-NEXT:    Binding: Global (0x1)
# CHECK-NEXT:    Type: None (0x0)
# CHECK-NEXT:    Other: 0
# CHECK-NEXT:    Section: .text
# CHECK-NEXT:  }
# CHECK-NEXT:  Symbol {
# CHECK-NEXT:    Name: f
# CHECK-NEXT:    Value: 0x202036
# CHECK-NEXT:    Size: 0
# CHECK-NEXT:    Binding: Global (0x1)
# CHECK-NEXT:    Type: None (0x0)
# CHECK-NEXT:    Other: 0
# CHECK-NEXT:    Section: .text
# CHECK-NEXT:  }
# CHECK-NEXT:  Symbol {
# CHECK-NEXT:    Name: g
# CHECK-NEXT:    Value: 0x201000
# CHECK-NEXT:    Size: 0
# CHECK-NEXT:    Binding: Global (0x1)
# CHECK-NEXT:    Type: None (0x0)
# CHECK-NEXT:    Other: 0
# CHECK-NEXT:    Section: .text
# CHECK-NEXT:  }
# CHECK-NEXT:  Symbol {
# CHECK-NEXT:    Name: h
# CHECK-NEXT:    Value: 0x201022
# CHECK-NEXT:    Size: 0
# CHECK-NEXT:    Binding: Global (0x1)
# CHECK-NEXT:    Type: None (0x0)
# CHECK-NEXT:    Other: 0
# CHECK-NEXT:    Section: .text
# CHECK-NEXT:  }
# CHECK-NEXT:]
