// RUN: %lfort_cc1 -emit-llvm %s  -o /dev/null

extern int vfork(void);
test() {
  vfork();
}
