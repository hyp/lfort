// RUN: %lfort_cc1 -fsyntax-only -verify -Wno-objc-root-class %s

@implementation INTF // expected-warning {{cannot find interface declaration for 'INTF'}}
@end

INTF* pi;

INTF* FUNC()
{
	return pi;
}
