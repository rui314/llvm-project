; RUN: llc < %s -march=nvptx -mcpu=sm_20 | FileCheck %s
; RUN: llc < %s -march=nvptx64 -mcpu=sm_20 | FileCheck %s
; RUN: opt < %s -S -mtriple=nvptx-nvidia-cuda -nvvm-intr-range \
; RUN:   | FileCheck --check-prefix=RANGE --check-prefix=RANGE_20 %s
; RUN: opt < %s -S -mtriple=nvptx-nvidia-cuda \
; RUN:    -nvvm-intr-range -nvvm-intr-range-sm=30 \
; RUN:   | FileCheck --check-prefix=RANGE --check-prefix=RANGE_30 %s

define ptx_device i32 @test_tid_x() {
; CHECK: mov.u32 %r{{[0-9]+}}, %tid.x;
; RANGE: call i32 @llvm.ptx.read.tid.x(), !range ![[BLK_IDX_XY:[0-9]+]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.tid.x()
	ret i32 %x
}

define ptx_device i32 @test_tid_y() {
; CHECK: mov.u32 %r{{[0-9]+}}, %tid.y;
; RANGE: call i32 @llvm.ptx.read.tid.y(), !range ![[BLK_IDX_XY]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.tid.y()
	ret i32 %x
}

define ptx_device i32 @test_tid_z() {
; CHECK: mov.u32 %r{{[0-9]+}}, %tid.z;
; RANGE: call i32 @llvm.ptx.read.tid.z(), !range ![[BLK_IDX_Z:[0-9]+]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.tid.z()
	ret i32 %x
}

define ptx_device i32 @test_tid_w() {
; CHECK: mov.u32 %r{{[0-9]+}}, %tid.w;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.tid.w()
	ret i32 %x
}

define ptx_device i32 @test_ntid_x() {
; CHECK: mov.u32 %r{{[0-9]+}}, %ntid.x;
; RANGE: call i32 @llvm.ptx.read.ntid.x(), !range ![[BLK_SIZE_XY:[0-9]+]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.ntid.x()
	ret i32 %x
}

define ptx_device i32 @test_ntid_y() {
; CHECK: mov.u32 %r{{[0-9]+}}, %ntid.y;
; RANGE: call i32 @llvm.ptx.read.ntid.y(), !range ![[BLK_SIZE_XY]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.ntid.y()
	ret i32 %x
}

define ptx_device i32 @test_ntid_z() {
; CHECK: mov.u32 %r{{[0-9]+}}, %ntid.z;
; RANGE: call i32 @llvm.ptx.read.ntid.z(), !range ![[BLK_SIZE_Z:[0-9]+]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.ntid.z()
	ret i32 %x
}

define ptx_device i32 @test_ntid_w() {
; CHECK: mov.u32 %r{{[0-9]+}}, %ntid.w;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.ntid.w()
	ret i32 %x
}

define ptx_device i32 @test_laneid() {
; CHECK: mov.u32 %r{{[0-9]+}}, %laneid;
; RANGE: call i32 @llvm.ptx.read.laneid(), !range ![[LANEID:[0-9]+]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.laneid()
	ret i32 %x
}

define ptx_device i32 @test_warpsize() {
; CHECK: mov.u32 %r{{[0-9]+}}, WARP_SZ;
; RANGE: call i32 @llvm.nvvm.read.ptx.sreg.warpsize(), !range ![[WARPSIZE:[0-9]+]]
; CHECK: ret;
	%x = call i32 @llvm.nvvm.read.ptx.sreg.warpsize()
	ret i32 %x
}

define ptx_device i32 @test_warpid() {
; CHECK: mov.u32 %r{{[0-9]+}}, %warpid;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.warpid()
	ret i32 %x
}

define ptx_device i32 @test_nwarpid() {
; CHECK: mov.u32 %r{{[0-9]+}}, %nwarpid;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.nwarpid()
	ret i32 %x
}

define ptx_device i32 @test_ctaid_y() {
; CHECK: mov.u32 %r{{[0-9]+}}, %ctaid.y;
; RANGE: call i32 @llvm.ptx.read.ctaid.y(), !range ![[GRID_IDX_YZ:[0-9]+]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.ctaid.y()
	ret i32 %x
}

define ptx_device i32 @test_ctaid_z() {
; CHECK: mov.u32 %r{{[0-9]+}}, %ctaid.z;
; RANGE: call i32 @llvm.ptx.read.ctaid.z(), !range ![[GRID_IDX_YZ]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.ctaid.z()
	ret i32 %x
}

define ptx_device i32 @test_ctaid_x() {
; CHECK: mov.u32 %r{{[0-9]+}}, %ctaid.x;
; RANGE_30: call i32 @llvm.ptx.read.ctaid.x(), !range ![[GRID_IDX_X:[0-9]+]]
; RANGE_20: call i32 @llvm.ptx.read.ctaid.x(), !range ![[GRID_IDX_YZ]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.ctaid.x()
	ret i32 %x
}

define ptx_device i32 @test_ctaid_w() {
; CHECK: mov.u32 %r{{[0-9]+}}, %ctaid.w;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.ctaid.w()
	ret i32 %x
}

define ptx_device i32 @test_nctaid_y() {
; CHECK: mov.u32 %r{{[0-9]+}}, %nctaid.y;
; RANGE: call i32 @llvm.ptx.read.nctaid.y(), !range ![[GRID_SIZE_YZ:[0-9]+]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.nctaid.y()
	ret i32 %x
}

define ptx_device i32 @test_nctaid_z() {
; CHECK: mov.u32 %r{{[0-9]+}}, %nctaid.z;
; RANGE: call i32 @llvm.ptx.read.nctaid.z(), !range ![[GRID_SIZE_YZ]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.nctaid.z()
	ret i32 %x
}

define ptx_device i32 @test_nctaid_x() {
; CHECK: mov.u32 %r{{[0-9]+}}, %nctaid.x;
; RANGE_30: call i32 @llvm.ptx.read.nctaid.x(), !range ![[GRID_SIZE_X:[0-9]+]]
; RANGE_20: call i32 @llvm.ptx.read.nctaid.x(), !range ![[GRID_SIZE_YZ]]
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.nctaid.x()
	ret i32 %x
}


define ptx_device i32 @test_nctaid_w() {
; CHECK: mov.u32 %r{{[0-9]+}}, %nctaid.w;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.nctaid.w()
	ret i32 %x
}

define ptx_device i32 @test_smid() {
; CHECK: mov.u32 %r{{[0-9]+}}, %smid;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.smid()
	ret i32 %x
}

define ptx_device i32 @test_nsmid() {
; CHECK: mov.u32 %r{{[0-9]+}}, %nsmid;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.nsmid()
	ret i32 %x
}

define ptx_device i32 @test_gridid() {
; CHECK: mov.u32 %r{{[0-9]+}}, %gridid;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.gridid()
	ret i32 %x
}

define ptx_device i32 @test_lanemask_eq() {
; CHECK: mov.u32 %r{{[0-9]+}}, %lanemask_eq;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.lanemask.eq()
	ret i32 %x
}

define ptx_device i32 @test_lanemask_le() {
; CHECK: mov.u32 %r{{[0-9]+}}, %lanemask_le;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.lanemask.le()
	ret i32 %x
}

define ptx_device i32 @test_lanemask_lt() {
; CHECK: mov.u32 %r{{[0-9]+}}, %lanemask_lt;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.lanemask.lt()
	ret i32 %x
}

define ptx_device i32 @test_lanemask_ge() {
; CHECK: mov.u32 %r{{[0-9]+}}, %lanemask_ge;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.lanemask.ge()
	ret i32 %x
}

define ptx_device i32 @test_lanemask_gt() {
; CHECK: mov.u32 %r{{[0-9]+}}, %lanemask_gt;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.lanemask.gt()
	ret i32 %x
}

define ptx_device i32 @test_clock() {
; CHECK: mov.u32 %r{{[0-9]+}}, %clock;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.clock()
	ret i32 %x
}

define ptx_device i64 @test_clock64() {
; CHECK: mov.u64 %rd{{[0-9]+}}, %clock64;
; CHECK: ret;
	%x = call i64 @llvm.ptx.read.clock64()
	ret i64 %x
}

define ptx_device i32 @test_pm0() {
; CHECK: mov.u32 %r{{[0-9]+}}, %pm0;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.pm0()
	ret i32 %x
}

define ptx_device i32 @test_pm1() {
; CHECK: mov.u32 %r{{[0-9]+}}, %pm1;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.pm1()
	ret i32 %x
}

define ptx_device i32 @test_pm2() {
; CHECK: mov.u32 %r{{[0-9]+}}, %pm2;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.pm2()
	ret i32 %x
}

define ptx_device i32 @test_pm3() {
; CHECK: mov.u32 %r{{[0-9]+}}, %pm3;
; CHECK: ret;
	%x = call i32 @llvm.ptx.read.pm3()
	ret i32 %x
}

define ptx_device void @test_bar_sync() {
; CHECK: bar.sync 0
; CHECK: ret;
	call void @llvm.ptx.bar.sync(i32 0)
	ret void
}

declare i32 @llvm.ptx.read.tid.x()
declare i32 @llvm.ptx.read.tid.y()
declare i32 @llvm.ptx.read.tid.z()
declare i32 @llvm.ptx.read.tid.w()
declare i32 @llvm.ptx.read.ntid.x()
declare i32 @llvm.ptx.read.ntid.y()
declare i32 @llvm.ptx.read.ntid.z()
declare i32 @llvm.ptx.read.ntid.w()

declare i32 @llvm.nvvm.read.ptx.sreg.warpsize()
declare i32 @llvm.ptx.read.laneid()
declare i32 @llvm.ptx.read.warpid()
declare i32 @llvm.ptx.read.nwarpid()

declare i32 @llvm.ptx.read.ctaid.x()
declare i32 @llvm.ptx.read.ctaid.y()
declare i32 @llvm.ptx.read.ctaid.z()
declare i32 @llvm.ptx.read.ctaid.w()
declare i32 @llvm.ptx.read.nctaid.x()
declare i32 @llvm.ptx.read.nctaid.y()
declare i32 @llvm.ptx.read.nctaid.z()
declare i32 @llvm.ptx.read.nctaid.w()

declare i32 @llvm.ptx.read.smid()
declare i32 @llvm.ptx.read.nsmid()
declare i32 @llvm.ptx.read.gridid()

declare i32 @llvm.ptx.read.lanemask.eq()
declare i32 @llvm.ptx.read.lanemask.le()
declare i32 @llvm.ptx.read.lanemask.lt()
declare i32 @llvm.ptx.read.lanemask.ge()
declare i32 @llvm.ptx.read.lanemask.gt()

declare i32 @llvm.ptx.read.clock()
declare i64 @llvm.ptx.read.clock64()

declare i32 @llvm.ptx.read.pm0()
declare i32 @llvm.ptx.read.pm1()
declare i32 @llvm.ptx.read.pm2()
declare i32 @llvm.ptx.read.pm3()

declare void @llvm.ptx.bar.sync(i32 %i)

; RANGE-DAG: ![[BLK_IDX_XY]] = !{i32 0, i32 1024}
; RANGE-DAG: ![[BLK_IDX_Z]] = !{i32 0, i32 64}
; RANGE-DAG: ![[BLK_SIZE_XY]] = !{i32 1, i32 1025}
; RANGE-DAG: ![[BLK_SIZE_Z]] = !{i32 1, i32 65}
; RANGE-DAG: ![[LANEID]] = !{i32 0, i32 32}
; RANGE-DAG: ![[WARPSIZE]] = !{i32 32, i32 33}
; RANGE_30-DAG: ![[GRID_IDX_X]] = !{i32 0, i32 2147483647}
; RANGE-DAG: ![[GRID_IDX_YZ]] = !{i32 0, i32 65535}
; RANGE_30-DAG: ![[GRID_SIZE_X]] = !{i32 1, i32 -2147483648}
; RANGE-DAG: ![[GRID_SIZE_YZ]] = !{i32 1, i32 65536}
