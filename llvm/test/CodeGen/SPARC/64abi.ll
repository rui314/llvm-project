; RUN: llc < %s -march=sparcv9 | FileCheck %s

; CHECK: intarg
; CHECK: stb %i0, [%i4]
; CHECK: stb %i1, [%i4]
; CHECK: sth %i2, [%i4]
; CHECK: st  %i3, [%i4]
; CHECK: stx %i4, [%i4]
; CHECK: st  %i5, [%i4]
; CHECK: ld [%fp+2227], [[R:%[gilo][0-7]]]
; CHECK: st  [[R]], [%i4]
; CHECK: ldx [%fp+2231], [[R:%[gilo][0-7]]]
; CHECK: stx [[R]], [%i4]
define void @intarg(i8  %a0,   ; %i0
                    i8  %a1,   ; %i1
                    i16 %a2,   ; %i2
                    i32 %a3,   ; %i3
                    i8* %a4,   ; %i4
                    i32 %a5,   ; %i5
                    i32 %a6,   ; [%fp+BIAS+176]
                    i8* %a7) { ; [%fp+BIAS+184]
  store i8 %a0, i8* %a4
  store i8 %a1, i8* %a4
  %p16 = bitcast i8* %a4 to i16*
  store i16 %a2, i16* %p16
  %p32 = bitcast i8* %a4 to i32*
  store i32 %a3, i32* %p32
  %pp = bitcast i8* %a4 to i8**
  store i8* %a4, i8** %pp
  store i32 %a5, i32* %p32
  store i32 %a6, i32* %p32
  store i8* %a7, i8** %pp
  ret void
}

; CHECK: floatarg
; CHECK: fstod %f1,
; CHECK: faddd %f2,
; CHECK: faddd %f4,
; CHECK: faddd %f6,
; CHECK: ld [%fp+2307], [[F:%f[0-9]+]]
; CHECK: fadds %f31, [[F]]
define double @floatarg(float %a0,    ; %f1
                        double %a1,   ; %d2
                        double %a2,   ; %d4
                        double %a3,   ; %d6
                        float %a4,    ; %f9
                        float %a5,    ; %f11
                        float %a6,    ; %f13
                        float %a7,    ; %f15
                        float %a8,    ; %f17
                        float %a9,    ; %f19
                        float %a10,   ; %f21
                        float %a11,   ; %f23
                        float %a12,   ; %f25
                        float %a13,   ; %f27
                        float %a14,   ; %f29
                        float %a15,   ; %f31
                        float %a16,   ; [%fp+BIAS+256] (using 8 bytes)
                        float %a17) { ; [%fp+BIAS+264] (using 8 bytes)
  %d0 = fpext float %a0 to double
  %s1 = fadd double %a1, %d0
  %s2 = fadd double %a2, %s1
  %s3 = fadd double %a3, %s2
  %s16 = fadd float %a15, %a16
  %d16 = fpext float %s16 to double
  %s17 = fadd double %d16, %s3
  ret double %s17
}

; CHECK: mixedarg
; CHECK: fstod %f3
; CHECK: faddd %f6
; CHECK: faddd %f16
; CHECK: ldx [%fp+2231]
; CHECK: ldx [%fp+2247]
define void @mixedarg(i8 %a0,      ; %i0
                      float %a1,   ; %f3
                      i16 %a2,     ; %i2
                      double %a3,  ; %d6
                      i13 %a4,     ; %i4
                      float %a5,   ; %f11
                      i64 %a6,     ; [%fp+BIAS+176]
                      double *%a7, ; [%fp+BIAS+184]
                      double %a8,  ; %d16
                      i16* %a9) {  ; [%fp+BIAS+200]
  %d1 = fpext float %a1 to double
  %s3 = fadd double %a3, %d1
  %s8 = fadd double %a8, %s3
  store double %s8, double* %a7
  store i16 %a2, i16* %a9
  ret void
}

; The inreg attribute is used to indicate 32-bit sized struct elements that
; share an 8-byte slot.
; CHECK: inreg_fi
; CHECK: fstoi %f1
; CHECK: srlx %i0, 32, [[R:%[gilo][0-7]]]
; CHECK: sub [[R]],
define i32 @inreg_fi(i32 inreg %a0,     ; high bits of %i0
                     float inreg %a1) { ; %f1
  %b1 = fptosi float %a1 to i32
  %rv = sub i32 %a0, %b1
  ret i32 %rv
}

; CHECK: inreg_ff
; CHECK: fsubs %f0, %f1, %f1
define float @inreg_ff(float inreg %a0,   ; %f0
                       float inreg %a1) { ; %f1
  %rv = fsub float %a0, %a1
  ret float %rv
}

; CHECK: inreg_if
; CHECK: fstoi %f0
; CHECK: sub %i0
define i32 @inreg_if(float inreg %a0, ; %f0
                     i32 inreg %a1) { ; low bits of %i0
  %b0 = fptosi float %a0 to i32
  %rv = sub i32 %a1, %b0
  ret i32 %rv
}

; The frontend shouldn't do this. Just pass i64 instead.
; CHECK: inreg_ii
; CHECK: srlx %i0, 32, [[R:%[gilo][0-7]]]
; CHECK: sub %i0, [[R]], %i0
define i32 @inreg_ii(i32 inreg %a0,   ; high bits of %i0
                     i32 inreg %a1) { ; low bits of %i0
  %rv = sub i32 %a1, %a0
  ret i32 %rv
}
