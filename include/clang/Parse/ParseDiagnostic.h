//===--- DiagnosticParse.h - Diagnostics for libparse -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LFORT_DIAGNOSTICPARSE_H
#define LLVM_LFORT_DIAGNOSTICPARSE_H

#include "lfort/Basic/Diagnostic.h"

namespace lfort {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define PARSESTART
#include "lfort/Basic/DiagnosticParseKinds.inc"
#undef DIAG
      NUM_BUILTIN_PARSE_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace lfort

#endif
