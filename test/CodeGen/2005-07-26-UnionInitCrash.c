// PR607
// RUN: %lfort_cc1 %s -emit-llvm -o -
union { char bytes[8]; double alignment; }EQ1 = {0,0,0,0,0,0,0,0};
