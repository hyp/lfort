//===--- LFortSACheckers.h - Registration functions for Checkers *- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Declares the registation functions for the checkers defined in
// liblfortStaticAnalyzerCheckers.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LFORT_SA_LIB_CHECKERS_LFORTSACHECKERS_H
#define LLVM_LFORT_SA_LIB_CHECKERS_LFORTSACHECKERS_H

#include "lfort/StaticAnalyzer/Checkers/CommonBugCategories.h"

namespace lfort {

namespace ento {
class CheckerManager;
class CheckerRegistry;

#define GET_CHECKERS
#define CHECKER(FULLNAME,CLASS,CXXFILE,HELPTEXT,GROUPINDEX,HIDDEN)    \
  void register##CLASS(CheckerManager &mgr);
#include "Checkers.inc"
#undef CHECKER
#undef GET_CHECKERS

} // end ento namespace

} // end lfort namespace

#endif
