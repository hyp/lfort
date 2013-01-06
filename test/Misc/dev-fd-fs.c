// Check that we can operate on files from /dev/fd.
// REQUIRES: dev-fd-fs

// It has not been working since r169831 on freebsd.
// XFAIL: freebsd

// Check reading from named pipes. We cat the input here instead of redirecting
// it to ensure that /dev/fd/0 is a named pipe, not just a redirected file.
//
// RUN: cat %s | %lfort -x c /dev/fd/0 -E > %t
// RUN: FileCheck --check-prefix DEV-FD-INPUT < %t %s
//
// DEV-FD-INPUT: int x;


// Check writing to /dev/fd named pipes. We use cat here as before to ensure we
// get a named pipe.
//
// RUN: %lfort -x c %s -E -o /dev/fd/1 | cat > %t
// RUN: FileCheck --check-prefix DEV-FD-FIFO-OUTPUT < %t %s
//
// DEV-FD-FIFO-OUTPUT: int x;


// Check writing to /dev/fd regular files.
//
// RUN: %lfort -x c %s -E -o /dev/fd/1 > %t
// RUN: FileCheck --check-prefix DEV-FD-REG-OUTPUT < %t %s
//
// DEV-FD-REG-OUTPUT: int x;

int x;
