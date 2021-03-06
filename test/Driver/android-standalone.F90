! Test header and library paths when LFort is used with Android standalone
! toolchain.
!
! RUN: %lfort -no-canonical-prefixes %s -### -o %t.o 2>&1 \
! RUN:     -target arm-linux-androideabi \
! RUN:     -B%S/Inputs/basic_android_tree \
! RUN:     --sysroot=%S/Inputs/basic_android_tree/sysroot \
! RUN:   | FileCheck  %s
! CHECK: {{.*}}lfort{{.*}}" "-cc1"
! CHECK: "-internal-externc-isystem" "{{.*}}/sysroot/include"
! CHECK: "-internal-externc-isystem" "{{.*}}/sysroot/usr/include"
! CHECK: "{{.*}}ld{{(.exe)?}}" "--sysroot=[[SYSROOT:[^"]+]]"
! CHECK: "-L{{.*}}/lib/gcc/arm-linux-androideabi/4.4.3"
! CHECK: "-L{{.*}}/lib/gcc/arm-linux-androideabi/4.4.3/../../../../arm-linux-androideabi/lib"
! CHECK: "-L{{.*}}/sysroot/usr/lib"
!
! RUN: %lfort -no-canonical-prefixes %s -### -o %t.o 2>&1 \
! RUN:     -target mipsel-linux-android \
! RUN:     -mips32 \
! RUN:     -B%S/Inputs/basic_android_tree \
! RUN:     --sysroot=%S/Inputs/basic_android_tree/sysroot \
! RUN:   | FileCheck --check-prefix=CHECK-MIPS %s
! CHECK-MIPS: {{.*}}lfort{{.*}}" "-cc1"
! CHECK-MIPS: "-internal-externc-isystem" "{{.*}}/sysroot/include"
! CHECK-MIPS: "-internal-externc-isystem" "{{.*}}/sysroot/usr/include"
! CHECK-MIPS: "{{.*}}ld{{(.exe)?}}" "--sysroot=[[SYSROOT:[^"]+]]"
! CHECK-MIPS: "-L{{.*}}/lib/gcc/mipsel-linux-android/4.4.3"
! CHECK-MIPS: "-L{{.*}}/lib/gcc/mipsel-linux-android/4.4.3/../../../../mipsel-linux-android/lib"
! CHECK-MIPS: "-L{{.*}}/sysroot/usr/lib"
!
! RUN: %lfort -no-canonical-prefixes %s -### -o %t.o 2>&1 \
! RUN:     -target mipsel-linux-android \
! RUN:     -march=mips32 -mips32r2 \
! RUN:     -B%S/Inputs/basic_android_tree \
! RUN:     --sysroot=%S/Inputs/basic_android_tree/sysroot \
! RUN:   | FileCheck --check-prefix=CHECK-MIPSR2 %s
! CHECK-MIPSR2: {{.*}}lfort{{.*}}" "-cc1"
! CHECK-MIPSR2: "-internal-externc-isystem" "{{.*}}/sysroot/include"
! CHECK-MIPSR2: "-internal-externc-isystem" "{{.*}}/sysroot/usr/include"
! CHECK-MIPSR2: "{{.*}}ld{{(.exe)?}}" "--sysroot=[[SYSROOT:[^"]+]]"
! CHECK-MIPSR2: "-L{{.*}}/lib/gcc/mipsel-linux-android/4.4.3/mips-r2"
! CHECK-MIPSR2: "-L{{.*}}/lib/gcc/mipsel-linux-android/4.4.3/../../../../mipsel-linux-android/lib"
! CHECK-MIPSR2: "-L{{.*}}/sysroot/usr/lib"
!
! RUN: %lfort -no-canonical-prefixes %s -### -o %t.o 2>&1 \
! RUN:     -target mipsel-linux-android \
! RUN:     -mips32 -march=mips32r2 \
! RUN:     -B%S/Inputs/basic_android_tree \
! RUN:     --sysroot=%S/Inputs/basic_android_tree/sysroot \
! RUN:   | FileCheck --check-prefix=CHECK-MIPSR2-A %s
! CHECK-MIPSR2-A: {{.*}}lfort{{.*}}" "-cc1"
! CHECK-MIPSR2-A: "-internal-externc-isystem" "{{.*}}/sysroot/include"
! CHECK-MIPSR2-A: "-internal-externc-isystem" "{{.*}}/sysroot/usr/include"
! CHECK-MIPSR2-A: "{{.*}}ld{{(.exe)?}}" "--sysroot=[[SYSROOT:[^"]+]]"
! CHECK-MIPSR2-A: "-L{{.*}}/lib/gcc/mipsel-linux-android/4.4.3/mips-r2"
! CHECK-MIPSR2-A: "-L{{.*}}/lib/gcc/mipsel-linux-android/4.4.3/../../../../mipsel-linux-android/lib"
! CHECK-MIPSR2-A: "-L{{.*}}/sysroot/usr/lib"
