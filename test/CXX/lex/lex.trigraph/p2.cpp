// RUN: %lfort_cc1 -fsyntax-only -trigraphs -Wtrigraphs -verify %s

??=define arraycheck(a,b) a??(b??) ??!??! b??(a??) // expected-warning {{trigraph converted to '#' character}} expected-warning {{trigraph converted to '[' character}} expected-warning {{trigraph converted to ']' character}} expected-warning {{trigraph converted to '|' character}} expected-warning {{trigraph converted to '|' character}} expected-warning {{trigraph converted to '[' character}} expected-warning {{trigraph converted to ']' character}}
