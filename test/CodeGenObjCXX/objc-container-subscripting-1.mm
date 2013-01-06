// RUN: %lfort_cc1 -emit-llvm -triple x86_64-apple-darwin -o - %s | FileCheck %s

typedef unsigned int size_t;
@protocol P @end
@protocol NSCopying @end

@interface NSMutableArray
- (id)objectAtIndexedSubscript:(size_t)index;
- (void)setObject:(id)object atIndexedSubscript:(size_t)index;
@end

struct S {
  operator unsigned int ();
  operator id ();
};

@interface NSMutableDictionary
- (id)objectForKeyedSubscript:(id<NSCopying>)key;
- (void)setObject:(id)object forKeyedSubscript:(id<NSCopying>)key;
@end

int main() {
  NSMutableArray<P> * array;
  S s;
  id oldObject = array[(int)s];

  NSMutableDictionary<P> *dict;
  dict[(id)s] = oldObject;
  oldObject = dict[(id)s];

}

template <class T> void test2(NSMutableArray *a) {
  a[10] = 0;
}
template void test2<int>(NSMutableArray*);
// CHECK: define weak_odr void @_Z5test2IiEvP14NSMutableArray
// CHECK: @objc_msgSend 
// CHECK: ret void


template <class T> void test3(NSMutableArray *a) {
  a[sizeof(T)] = 0;
}

template void test3<int>(NSMutableArray*);
// CHECK: define weak_odr void @_Z5test3IiEvP14NSMutableArray
// CHECK: @objc_msgSend
// CHECK: ret void

