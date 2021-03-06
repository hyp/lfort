//===---- CheckerHelpers.cpp - Helper functions for checkers ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines several static functions for use in checkers.
//
//===----------------------------------------------------------------------===//

#include "lfort/StaticAnalyzer/Core/PathSensitive/CheckerHelpers.h"
#include "lfort/AST/Expr.h"

// Recursively find any substatements containing macros
bool lfort::ento::containsMacro(const Stmt *S) {
  if (S->getLocStart().isMacroID())
    return true;

  if (S->getLocEnd().isMacroID())
    return true;

  for (Stmt::const_child_iterator I = S->child_begin(); I != S->child_end();
      ++I)
    if (const Stmt *child = *I)
      if (containsMacro(child))
        return true;

  return false;
}

// Recursively find any substatements containing enum constants
bool lfort::ento::containsEnum(const Stmt *S) {
  const DeclRefExpr *DR = dyn_cast<DeclRefExpr>(S);

  if (DR && isa<EnumConstantDecl>(DR->getDecl()))
    return true;

  for (Stmt::const_child_iterator I = S->child_begin(); I != S->child_end();
      ++I)
    if (const Stmt *child = *I)
      if (containsEnum(child))
        return true;

  return false;
}

// Recursively find any substatements containing static vars
bool lfort::ento::containsStaticLocal(const Stmt *S) {
  const DeclRefExpr *DR = dyn_cast<DeclRefExpr>(S);

  if (DR)
    if (const VarDecl *VD = dyn_cast<VarDecl>(DR->getDecl()))
      if (VD->isStaticLocal())
        return true;

  for (Stmt::const_child_iterator I = S->child_begin(); I != S->child_end();
      ++I)
    if (const Stmt *child = *I)
      if (containsStaticLocal(child))
        return true;

  return false;
}

// Recursively find any substatements containing __builtin_offsetof
bool lfort::ento::containsBuiltinOffsetOf(const Stmt *S) {
  if (isa<OffsetOfExpr>(S))
    return true;

  for (Stmt::const_child_iterator I = S->child_begin(); I != S->child_end();
      ++I)
    if (const Stmt *child = *I)
      if (containsBuiltinOffsetOf(child))
        return true;

  return false;
}
