// RUN: %lfort_cc1 %s -verify -fsyntax-only
// vim: set binary noeol:

// This file intentionally ends without a \n on the last line.  Make sure your
// editor doesn't add one.

// expected-error@+1{{expected expression}} expected-error@+1{{expected ';'}}
char c = \