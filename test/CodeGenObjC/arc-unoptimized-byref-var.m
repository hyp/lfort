// RUN: %lfort_cc1 -triple x86_64-apple-darwin10 -emit-llvm -fblocks -fobjc-arc -fobjc-runtime-has-weak -o - %s | FileCheck -check-prefix=CHECK-UNOPT %s
// rdar://12530881

void test19() {
   __block id x;
// CHECK-UNOPT: define internal void @__Block_byref_object_copy
// CHECK-UNOPT: [[X:%.*]] = getelementptr inbounds [[BYREF_T:%.*]]* [[VAR:%.*]], i32 0, i32 6
// CHECK-UNOPT: [[X2:%.*]] = getelementptr inbounds [[BYREF_T:%.*]]* [[VAR1:%.*]], i32 0, i32 6
// CHECK-UNOPT-NEXT: [[SIX:%.*]] = load i8** [[X2]], align 8
// CHECK-UNOPT-NEXT: store i8* null, i8** [[X]], align 8
// CHECK-UNOPT-NEXT: call void @objc_storeStrong(i8** [[X]], i8* [[SIX]]) nounwind
// CHECK-UNOPT-NEXT: call void @objc_storeStrong(i8** [[X2]], i8* null) nounwind
// CHECK-UNOPT-NEXT: ret void
}

