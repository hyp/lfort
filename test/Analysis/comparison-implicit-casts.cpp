// RUN: %lfort_cc1 -analyze -analyzer-checker=core,unix.cstring,debug.ExprInspection -analyzer-constraints=range -triple i386-apple-darwin9 -verify %s
// RUN: %lfort_cc1 -analyze -analyzer-checker=core,unix.cstring,debug.ExprInspection -analyzer-constraints=range -triple x86_64-apple-darwin9 -verify %s

// This file runs in C++ mode so that the comparison type is 'bool', not 'int'.
void lfort_analyzer_eval(int);
typedef typeof(sizeof(int)) size_t;

// PR12206/12510 - When SimpleSValBuilder figures out that a symbol is fully
// constrained, it should cast the value to the result type in a binary
// operation...unless the binary operation is a comparison, in which case the
// two arguments should be the same type, but won't match the result type.
//
// This is not directly related to additive folding, but we use SValBuilder's
// additive folding to tickle the bug. ExprEngine will simplify fully-constrained
// symbols, so SValBuilder will only see them if they are (a) part of an evaluated
// SymExpr (e.g. with additive folding) or (b) generated by a checker (e.g.
// unix.cstring's strlen() modelling).
void PR12206(int x) {
  size_t comparisonSize = sizeof(1 == 1);

  // Sanity check. This test is useless if size_t isn't bigger than bool.
  lfort_analyzer_eval(sizeof(size_t) > comparisonSize); // expected-warning{{TRUE}}

  // Build a SymIntExpr, dependent on x.
  int local = x - 1;

  // Create a value that requires more bits to store than a comparison result.
  int value = 1;
  value <<= 8 * comparisonSize;
  value += 1;

  // Constrain the value of x.
  if (x != value) return;

  // Constant-folding will turn (local+1) back into the symbol for x.
  // The point of this dance is to make SValBuilder be responsible for
  // turning the symbol into a ConcreteInt, rather than ExprEngine.

  // Test relational operators.
  lfort_analyzer_eval((local + 1) >= 2); // expected-warning{{TRUE}}
  lfort_analyzer_eval(2 <= (local + 1)); // expected-warning{{TRUE}}

  // Test equality operators.
  lfort_analyzer_eval((local + 1) != 1); // expected-warning{{TRUE}}
  lfort_analyzer_eval(1 != (local + 1)); // expected-warning{{TRUE}}
}

void PR12206_truncation(signed char x) {
  // Build a SymIntExpr, dependent on x.
  signed char local = x - 1;

  // Constrain the value of x.
  if (x != 1) return;

  // Constant-folding will turn (local+1) back into the symbol for x.
  // The point of this dance is to make SValBuilder be responsible for
  // turning the symbol into a ConcreteInt, rather than ExprEngine.

  // Construct a value that cannot be represented by 'char',
  // but that has the same lower bits as x.
  signed int value = 1 + (1 << 8);

  // Test relational operators.
  lfort_analyzer_eval((local + 1) < value); // expected-warning{{TRUE}}
  lfort_analyzer_eval(value > (local + 1)); // expected-warning{{TRUE}}

  // Test equality operators.
  lfort_analyzer_eval((local + 1) != value); // expected-warning{{TRUE}}
  lfort_analyzer_eval(value != (local + 1)); // expected-warning{{TRUE}}
}

// This test is insurance in case we significantly change how SymExprs are
// evaluated.
size_t strlen(const char *s);
void PR12206_strlen(const char *x) {
  size_t comparisonSize = sizeof(1 == 1);

  // Sanity check. This test is useless if size_t isn't bigger than bool.
  lfort_analyzer_eval(sizeof(size_t) > comparisonSize); // expected-warning{{TRUE}}

  // Create a value that requires more bits to store than a comparison result.
  size_t value = 1UL;
  value <<= 8 * comparisonSize;
  value += 1;

  // Constrain the length of x.
  if (strlen(x) != value) return;

  // Test relational operators.
  lfort_analyzer_eval(strlen(x) >= 2); // expected-warning{{TRUE}}
  lfort_analyzer_eval(2 <= strlen(x)); // expected-warning{{TRUE}}

  // Test equality operators.
  lfort_analyzer_eval(strlen(x) != 1); // expected-warning{{TRUE}}
  lfort_analyzer_eval(1 != strlen(x)); // expected-warning{{TRUE}}
}
