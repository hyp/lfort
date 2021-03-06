! General tests that the header search paths detected by the driver and passed
! to CC1 are sane.
!
! Test a very broken version of multiarch that shipped in Ubuntu 11.04.
! RUN: %lfort -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
! RUN:     -target i386-unknown-linux \
! RUN:     --sysroot=%S/Inputs/ubuntu_11.04_multiarch_tree \
! RUN:   | FileCheck --check-prefix=CHECK-UBUNTU-11-04 %s
! CHECK-UBUNTU-11-04: "{{.*}}lfort{{.*}}" "-cc1"
! CHECK-UBUNTU-11-04: "-isysroot" "[[SYSROOT:[^"]+]]"
! CHECK-UBUNTU-11-04: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
! CHECK-UBUNTU-11-04: "-internal-isystem" "{{.*}}/lib{{(64|32)?}}/lfort/{{[0-9]\.[0-9]}}/include"
! CHECK-UBUNTU-11-04: "-internal-externc-isystem" "[[SYSROOT]]/include"
! CHECK-UBUNTU-11-04: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
!
! Thoroughly exercise the Debian multiarch environment.
! RUN: %lfort -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
! RUN:     -target i686-linux-gnu \
! RUN:     --sysroot=%S/Inputs/debian_multiarch_tree \
! RUN:   | FileCheck --check-prefix=CHECK-DEBIAN-X86 %s
! CHECK-DEBIAN-X86: "{{[^"]*}}lfort{{[^"]*}}" "-cc1"
! CHECK-DEBIAN-X86: "-isysroot" "[[SYSROOT:[^"]+]]"
! CHECK-DEBIAN-X86: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
! CHECK-DEBIAN-X86: "-internal-isystem" "{{.*}}/lib{{(64|32)?}}/lfort/{{[0-9]\.[0-9]}}/include"
! CHECK-DEBIAN-X86: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/i386-linux-gnu"
! CHECK-DEBIAN-X86: "-internal-externc-isystem" "[[SYSROOT]]/include"
! CHECK-DEBIAN-X86: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
! RUN: %lfort -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
! RUN:     -target x86_64-linux-gnu \
! RUN:     --sysroot=%S/Inputs/debian_multiarch_tree \
! RUN:   | FileCheck --check-prefix=CHECK-DEBIAN-X86-64 %s
! CHECK-DEBIAN-X86-64: "{{[^"]*}}lfort{{[^"]*}}" "-cc1"
! CHECK-DEBIAN-X86-64: "-isysroot" "[[SYSROOT:[^"]+]]"
! CHECK-DEBIAN-X86-64: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
! CHECK-DEBIAN-X86-64: "-internal-isystem" "{{.*}}/lib{{(64|32)?}}/lfort/{{[0-9]\.[0-9]}}/include"
! CHECK-DEBIAN-X86-64: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/x86_64-linux-gnu"
! CHECK-DEBIAN-X86-64: "-internal-externc-isystem" "[[SYSROOT]]/include"
! CHECK-DEBIAN-X86-64: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
! RUN: %lfort -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
! RUN:     -target powerpc-linux-gnu \
! RUN:     --sysroot=%S/Inputs/debian_multiarch_tree \
! RUN:   | FileCheck --check-prefix=CHECK-DEBIAN-PPC %s
! CHECK-DEBIAN-PPC: "{{[^"]*}}lfort{{[^"]*}}" "-cc1"
! CHECK-DEBIAN-PPC: "-isysroot" "[[SYSROOT:[^"]+]]"
! CHECK-DEBIAN-PPC: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
! CHECK-DEBIAN-PPC: "-internal-isystem" "{{.*}}/lib{{(64|32)?}}/lfort/{{[0-9]\.[0-9]}}/include"
! CHECK-DEBIAN-PPC: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/powerpc-linux-gnu"
! CHECK-DEBIAN-PPC: "-internal-externc-isystem" "[[SYSROOT]]/include"
! CHECK-DEBIAN-PPC: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
! RUN: %lfort -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
! RUN:     -target powerpc64-linux-gnu \
! RUN:     --sysroot=%S/Inputs/debian_multiarch_tree \
! RUN:   | FileCheck --check-prefix=CHECK-DEBIAN-PPC64 %s
! CHECK-DEBIAN-PPC64: "{{[^"]*}}lfort{{[^"]*}}" "-cc1"
! CHECK-DEBIAN-PPC64: "-isysroot" "[[SYSROOT:[^"]+]]"
! CHECK-DEBIAN-PPC64: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
! CHECK-DEBIAN-PPC64: "-internal-isystem" "{{.*}}/lib{{(64|32)?}}/lfort/{{[0-9]\.[0-9]}}/include"
! CHECK-DEBIAN-PPC64: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/powerpc64-linux-gnu"
! CHECK-DEBIAN-PPC64: "-internal-externc-isystem" "[[SYSROOT]]/include"
! CHECK-DEBIAN-PPC64: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
