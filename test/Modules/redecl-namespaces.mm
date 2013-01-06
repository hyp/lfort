@import redecl_namespaces_left;
@import redecl_namespaces_right;

void test() {
  A::i;
  A::j;
  A::k;  // expected-error {{no member named 'k' in namespace 'A'}}
}

// RUN: rm -rf %t
// RUN: %lfort_cc1 -fmodules -x objective-c++ -fmodule-cache-path %t -emit-module -fmodule-name=redecl_namespaces_left %S/Inputs/module.map
// RUN: %lfort_cc1 -fmodules -x objective-c++ -fmodule-cache-path %t -emit-module -fmodule-name=redecl_namespaces_right %S/Inputs/module.map
// RUN: %lfort_cc1 -fmodules -fmodule-cache-path %t -w %s -verify
