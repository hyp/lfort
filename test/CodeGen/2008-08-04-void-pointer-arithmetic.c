// RUN: %lfort_cc1 -emit-llvm -o - %s
// <rdar://problem/6122967>

int f0(void *a, void *b) {
  return a - b;
}
