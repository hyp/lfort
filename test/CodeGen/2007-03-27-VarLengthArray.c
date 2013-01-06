// RUN: %lfort_cc1 -emit-llvm %s -o - | FileCheck %s

// CHECK: getelementptr inbounds i32* %{{vla|[0-9]}}
extern void f(int *);
int e(int m, int n) {
  int x[n];
  f(x);
  return x[m];
}
