! RUN: %lfort -### -Wlarge-by-value-copy %s 2>&1 | FileCheck -check-prefix=LARGE_VALUE_COPY_DEFAULT %s
! LARGE_VALUE_COPY_DEFAULT: -Wlarge-by-value-copy=64
! RUN: %lfort -### -Wlarge-by-value-copy=128 %s 2>&1 | FileCheck -check-prefix=LARGE_VALUE_COPY_JOINED %s
! LARGE_VALUE_COPY_JOINED: -Wlarge-by-value-copy=128

! RUN: %lfort -### -c -Wmonkey -Wno-monkey -Wno-unused-command-line-arguments \
! RUN:        -Wno-unused-command-line-argument %s 2>&1 | FileCheck %s
! CHECK: unknown warning option '-Wmonkey'
! CHECK: unknown warning option '-Wno-monkey'
! CHECK: unknown warning option '-Wno-unused-command-line-arguments'; did you mean '-Wno-unused-command-line-argument'?

! FIXME: Remove this together with -Warc-abi once an Xcode is released that doesn't pass this flag.
! RUN: %lfort -### -Warc-abi -Wno-arc-abi %s 2>&1 | FileCheck -check-prefix=ARCABI %s
! ARCABI-NOT: unknown warning option '-Warc-abi'
! ARCABI-NOT: unknown warning option '-Wno-arc-abi'

! Check that -isysroot warns on nonexistent paths.
! RUN: %lfort -### -c -target i386-apple-darwin10 -isysroot /FOO %s 2>&1 | FileCheck --check-prefix=CHECK-ISYSROOT %s
! CHECK-ISYSROOT: warning: no such sysroot directory: '{{([A-Za-z]:.*)?}}/FOO'
