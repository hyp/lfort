/* RUN: %lfort_cc1 -E %s | grep 'a 3'
 * RUN: %lfort_cc1 -E %s | grep 'b 16'
 * RUN: %lfort_cc1 -E -P %s | grep 'a 3'
 * RUN: %lfort_cc1 -E -P %s | grep 'b 16'
 * RUN: %lfort_cc1 -E %s | not grep '# 0 '
 * RUN: %lfort_cc1 -E -P %s | count 4
 * PR1848 PR3437 PR7360
*/

#define t(x) x

t(a
3)

t(b
__LINE__)

