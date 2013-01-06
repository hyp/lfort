// RUN: %lfort -target x86_64-apple-darwin10 --relocatable-pch -o %t \
// RUN:   -isysroot %S/libroot %S/libroot/usr/include/reloc.h
// RUN: %lfort -target x86_64-apple-darwin10 -fsyntax-only \
// RUN:   -include-pch %t -isysroot %S/libroot %s -Xlfort -verify
// RUN: not %lfort -target x86_64-apple-darwin10 -include-pch %t %s

#include <reloc.h>

int x = 2; // expected-error{{redefinition}}
int y = 5; // expected-error{{redefinition}}


// expected-note{{previous definition}}
// expected-note{{previous definition}}
