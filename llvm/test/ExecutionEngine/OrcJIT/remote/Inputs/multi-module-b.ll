declare i32 @FC()

define i32 @FB() nounwind {
  %r = call i32 @FC( )   ; <i32> [#uses=1]
  ret i32 %r
}

