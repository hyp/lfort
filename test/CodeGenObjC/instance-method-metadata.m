// REQUIRES: x86-64-registered-target
// RUN: %lfort_cc1 -triple x86_64-apple-darwin10 -S -o %t %s 
// RUN: FileCheck < %t %s

// rdar://9072317

/** The problem looks like lfort getting confused when a single translation unit 
    contains a protocol with a property and two classes that implement that protocol 
    and synthesize the property.
*/

@protocol Proto
@property (assign) id prop;
@end

@interface NSObject @end

@interface Foo : NSObject <Proto> { int x; } @end

@interface Bar : NSObject <Proto> @end

@implementation Foo
@synthesize prop;
@end

@implementation Bar
@synthesize prop;
@end

// CHECK: l_OBJC_$_INSTANCE_METHODS_Bar:
// CHECK-NEXT:        .long   24
// CHECK-NEXT:        .long   2
// CHECK-NEXT:        .quad   L_OBJC_METH_VAR_NAME_
// CHECK-NEXT:        .quad   L_OBJC_METH_VAR_TYPE_
// CHECK-NEXT:        .quad   "-[Bar prop]"
