// PR 1346
// RUN: %lfort_cc1 -emit-llvm %s  -o /dev/null
extern bar(void *);

void f(void *cd) {
  bar(((void *)((unsigned long)(cd) ^ -1)));
}
