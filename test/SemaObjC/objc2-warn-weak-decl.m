// RUN: %lfort_cc1 -triple i386-apple-darwin9 -fsyntax-only -fobjc-gc -verify %s
// RUN: %lfort_cc1 -x objective-c++ -triple i386-apple-darwin9 -fsyntax-only -fobjc-gc -verify %s
struct S {
	__weak id  p;  // expected-warning {{__weak attribute cannot be specified on a field declaration}}
};

int main ()
{
  __weak id  local;  // expected-warning {{Objective-C GC does not allow weak variables on the stack}}
}

