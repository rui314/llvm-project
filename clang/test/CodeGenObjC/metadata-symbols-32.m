// RUN: %clang_cc1 -triple i386-apple-darwin9 -fobjc-runtime=macosx-fragile-10.5 -emit-llvm -o %t %s

// RUN: grep '@"\\01L_OBJC_CATEGORY_A_Cat" = internal global .*section "__OBJC,__category,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_CATEGORY_CLASS_METHODS_A_Cat" = internal global .*section "__OBJC,__cat_cls_meth,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_CATEGORY_INSTANCE_METHODS_A_Cat" = internal global .*section "__OBJC,__cat_inst_meth,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_CLASSEXT_A" = internal global .*section "__OBJC,__class_ext,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_CLASS_A" = internal global .*section "__OBJC,__class,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_CLASS_METHODS_A" = internal global .*section "__OBJC,__cls_meth,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_CLASS_NAME_[0-9]*" = internal global .*section "__TEXT,__cstring,cstring_literals", align 1' %t
// RUN: grep '@"\\01L_OBJC_CLASS_PROTOCOLS_A" = internal global .*section "__OBJC,__cat_cls_meth,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_CLASS_REFERENCES_[0-9]*" = internal global .*section "__OBJC,__cls_refs,literal_pointers,no_dead_strip", align 4' %t

// Clang's Obj-C 32-bit doesn't emit ivars for the root class.
// RUNX: grep '@"\\01L_OBJC_CLASS_VARIABLES_A" = internal global .*section "__OBJC,__class_vars,regular,no_dead_strip", align 4' %t && 

// RUN: grep '@"\\01L_OBJC_INSTANCE_METHODS_A" = internal global .*section "__OBJC,__inst_meth,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_INSTANCE_VARIABLES_A" = internal global .*section "__OBJC,__instance_vars,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_METACLASS_A" = internal global .*section "__OBJC,__meta_class,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_METH_VAR_NAME_[0-9]*" = internal global .*section "__TEXT,__cstring,cstring_literals", align 1' %t
// RUN: grep '@"\\01L_OBJC_METH_VAR_TYPE_[0-9]*" = internal global .*section "__TEXT,__cstring,cstring_literals", align 1' %t
// RUN: grep '@"\\01L_OBJC_MODULES" = internal global .*section "__OBJC,__module_info,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_PROP_NAME_ATTR_[0-9]*" = internal global .*section "__TEXT,__cstring,cstring_literals", align 1' %t
// RUN: grep '@"\\01L_OBJC_PROTOCOL_CLASS_METHODS_P" = internal global .*section "__OBJC,__cat_cls_meth,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_PROTOCOL_INSTANCE_METHODS_P" = internal global .*section "__OBJC,__cat_inst_meth,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_PROTOCOL_P" = internal global .*section "__OBJC,__protocol,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_SELECTOR_REFERENCES_[0-9]*" = internal externally_initialized global .*section "__OBJC,__message_refs,literal_pointers,no_dead_strip", align 4' %t
// RUN: grep '@"\\01L_OBJC_SYMBOLS" = internal global .*section "__OBJC,__symbols,regular,no_dead_strip", align 4' %t
// RUN: grep '@"\\01l_OBJC_$_PROP_LIST_A" = internal global .*section "__OBJC,__property,regular,no_dead_strip", align 4' %t
// RUN: grep "\.lazy_reference \.objc_class_name_J0" %t


/*

Here is a handy command for looking at llvm-gcc's output:
llvm-gcc -m32 -emit-llvm -S -o - metadata-symbols-32.m | \
  grep '=.*global' | \
  sed -e 's#global.*, section#global ... section#' | \
  sort

*/

@interface B
@end
@interface C
@end

@protocol P
+(void) fm0;
-(void) im0;
@end

@interface A<P> {
  int _ivar;
}
 
@property (assign) int ivar;

+(void) fm0;
-(void) im0;
@end

@implementation A
@synthesize ivar = _ivar;
+(void) fm0 {
}
-(void) im0 {
}
@end

@implementation A (Cat)
+(void) fm1 {
}
-(void) im1 {
}
@end

@interface J0
@end

@implementation J0(Category) @end

void *f0() {
   [B im0];
   [C im1];
}

