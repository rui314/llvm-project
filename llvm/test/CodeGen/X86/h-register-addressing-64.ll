; RUN: llc < %s -march=x86-64 -mattr=-bmi | grep "movzbl	%[abcd]h," | count 7

; Use h-register extract and zero-extend.

define double @foo8(double* nocapture inreg %p, i64 inreg %x) nounwind readonly {
  %t0 = lshr i64 %x, 8
  %t1 = and i64 %t0, 255
  %t2 = getelementptr double* %p, i64 %t1
  %t3 = load double* %t2, align 8
  ret double %t3
}
define float @foo4(float* nocapture inreg %p, i64 inreg %x) nounwind readonly {
  %t0 = lshr i64 %x, 8
  %t1 = and i64 %t0, 255
  %t2 = getelementptr float* %p, i64 %t1
  %t3 = load float* %t2, align 8
  ret float %t3
}
define i16 @foo2(i16* nocapture inreg %p, i64 inreg %x) nounwind readonly {
  %t0 = lshr i64 %x, 8
  %t1 = and i64 %t0, 255
  %t2 = getelementptr i16* %p, i64 %t1
  %t3 = load i16* %t2, align 8
  ret i16 %t3
}
define i8 @foo1(i8* nocapture inreg %p, i64 inreg %x) nounwind readonly {
  %t0 = lshr i64 %x, 8
  %t1 = and i64 %t0, 255
  %t2 = getelementptr i8* %p, i64 %t1
  %t3 = load i8* %t2, align 8
  ret i8 %t3
}
define i8 @bar8(i8* nocapture inreg %p, i64 inreg %x) nounwind readonly {
  %t0 = lshr i64 %x, 5
  %t1 = and i64 %t0, 2040
  %t2 = getelementptr i8* %p, i64 %t1
  %t3 = load i8* %t2, align 8
  ret i8 %t3
}
define i8 @bar4(i8* nocapture inreg %p, i64 inreg %x) nounwind readonly {
  %t0 = lshr i64 %x, 6
  %t1 = and i64 %t0, 1020
  %t2 = getelementptr i8* %p, i64 %t1
  %t3 = load i8* %t2, align 8
  ret i8 %t3
}
define i8 @bar2(i8* nocapture inreg %p, i64 inreg %x) nounwind readonly {
  %t0 = lshr i64 %x, 7
  %t1 = and i64 %t0, 510
  %t2 = getelementptr i8* %p, i64 %t1
  %t3 = load i8* %t2, align 8
  ret i8 %t3
}
