// RUN: %lfort_cc1 %s -E | grep 'V);'
// RUN: %lfort_cc1 %s -E | grep 'W, 1, 2);'
// RUN: %lfort_cc1 %s -E | grep 'X, 1, 2);'
// RUN: %lfort_cc1 %s -E | grep 'Y, );'
// RUN: %lfort_cc1 %s -E | grep 'Z, );'

#define debug(format, ...) format, ## __VA_ARGS__)
debug(V);
debug(W, 1, 2);
debug(X, 1, 2 );
debug(Y, );
debug(Z,);

