; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: llc -mtriple=x86_64-unknown-unknown < %s | FileCheck %s

; cmp with single-use load, should not form branch.
define i32 @test1(double %a, double* nocapture %b, i32 %x, i32 %y)  {
; CHECK-LABEL: test1:
; CHECK:       # BB#0:
; CHECK-NEXT:    ucomisd (%rdi), %xmm0
; CHECK-NEXT:    cmovbel %edx, %esi
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
;
  %load = load double, double* %b, align 8
  %cmp = fcmp olt double %load, %a
  %cond = select i1 %cmp, i32 %x, i32 %y
  ret i32 %cond
}

; Sanity check: no load.
define i32 @test2(double %a, double %b, i32 %x, i32 %y)  {
; CHECK-LABEL: test2:
; CHECK:       # BB#0:
; CHECK-NEXT:    ucomisd %xmm1, %xmm0
; CHECK-NEXT:    cmovbel %esi, %edi
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    retq
;
  %cmp = fcmp ogt double %a, %b
  %cond = select i1 %cmp, i32 %x, i32 %y
  ret i32 %cond
}

; Multiple uses of the load.
define i32 @test4(i32 %a, i32* nocapture %b, i32 %x, i32 %y)  {
; CHECK-LABEL: test4:
; CHECK:       # BB#0:
; CHECK-NEXT:    movl (%rsi), %eax
; CHECK-NEXT:    cmpl %edi, %eax
; CHECK-NEXT:    cmovael %ecx, %edx
; CHECK-NEXT:    addl %edx, %eax
; CHECK-NEXT:    retq
;
  %load = load i32, i32* %b, align 4
  %cmp = icmp ult i32 %load, %a
  %cond = select i1 %cmp, i32 %x, i32 %y
  %add = add i32 %cond, %load
  ret i32 %add
}

; Multiple uses of the cmp.
define i32 @test5(i32 %a, i32* nocapture %b, i32 %x, i32 %y) {
; CHECK-LABEL: test5:
; CHECK:       # BB#0:
; CHECK-NEXT:    cmpl %edi, (%rsi)
; CHECK-NEXT:    cmoval %edi, %ecx
; CHECK-NEXT:    cmovael %edx, %ecx
; CHECK-NEXT:    movl %ecx, %eax
; CHECK-NEXT:    retq
;
  %load = load i32, i32* %b, align 4
  %cmp = icmp ult i32 %load, %a
  %cmp1 = icmp ugt i32 %load, %a
  %cond = select i1 %cmp1, i32 %a, i32 %y
  %cond5 = select i1 %cmp, i32 %cond, i32 %x
  ret i32 %cond5
}

; If a select is not obviously predictable, don't turn it into a branch.
define i32 @weighted_select1(i32 %a, i32 %b) {
; CHECK-LABEL: weighted_select1:
; CHECK:       # BB#0:
; CHECK-NEXT:    testl %edi, %edi
; CHECK-NEXT:    cmovnel %edi, %esi
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
;
  %cmp = icmp ne i32 %a, 0
  %sel = select i1 %cmp, i32 %a, i32 %b, !prof !0
  ret i32 %sel
}

; If a select is obviously predictable, turn it into a branch.
define i32 @weighted_select2(i32 %a, i32 %b) {
; CHECK-LABEL: weighted_select2:
; CHECK:       # BB#0:
; CHECK-NEXT:    testl %edi, %edi
; CHECK-NEXT:    jne [[LABEL_BB5:.*]]
; CHECK:         movl %esi, %edi
; CHECK-NEXT:  [[LABEL_BB5]]
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    retq
;
  %cmp = icmp ne i32 %a, 0
  %sel = select i1 %cmp, i32 %a, i32 %b, !prof !1
  ret i32 %sel
}

; Note the reversed profile weights: it doesn't matter if it's
; obviously true or obviously false.
; Either one should become a branch rather than conditional move.
; TODO: But likely true vs. likely false should affect basic block placement?
define i32 @weighted_select3(i32 %a, i32 %b) {
; CHECK-LABEL: weighted_select3:
; CHECK:       # BB#0:
; CHECK-NEXT:    testl %edi, %edi
; CHECK-NEXT:    je [[LABEL_BB6:.*]]
; CHECK:         movl %edi, %eax
; CHECK-NEXT:    retq
; CHECK:         [[LABEL_BB6]]
; CHECK-NEXT:    movl %esi, %edi
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    retq
;
  %cmp = icmp ne i32 %a, 0
  %sel = select i1 %cmp, i32 %a, i32 %b, !prof !2
  ret i32 %sel
}

; Weightlessness is no reason to die.
define i32 @unweighted_select(i32 %a, i32 %b) {
; CHECK-LABEL: unweighted_select:
; CHECK:       # BB#0:
; CHECK-NEXT:    testl %edi, %edi
; CHECK-NEXT:    cmovnel %edi, %esi
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
;
  %cmp = icmp ne i32 %a, 0
  %sel = select i1 %cmp, i32 %a, i32 %b, !prof !3
  ret i32 %sel
}

!0 = !{!"branch_weights", i32 1, i32 99}
!1 = !{!"branch_weights", i32 1, i32 100}
!2 = !{!"branch_weights", i32 100, i32 1}
!3 = !{!"branch_weights", i32 0, i32 0}

