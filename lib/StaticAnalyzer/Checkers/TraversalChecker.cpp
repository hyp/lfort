//== TraversalChecker.cpp -------------------------------------- -*- C++ -*--=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These checkers print various aspects of the ExprEngine's traversal of the CFG
// as it builds the ExplodedGraph.
//
//===----------------------------------------------------------------------===//
#include "LFortSACheckers.h"
#include "lfort/AST/ParentMap.h"
#include "lfort/AST/StmtObjC.h"
#include "lfort/StaticAnalyzer/Core/Checker.h"
#include "lfort/StaticAnalyzer/Core/CheckerManager.h"
#include "lfort/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "lfort/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "llvm/Support/raw_ostream.h"

using namespace lfort;
using namespace ento;

namespace {
class TraversalDumper : public Checker< check::BranchCondition,
                                        check::EndSubprogram > {
public:
  void checkBranchCondition(const Stmt *Condition, CheckerContext &C) const;
  void checkEndSubprogram(CheckerContext &C) const;
};
}

void TraversalDumper::checkBranchCondition(const Stmt *Condition,
                                           CheckerContext &C) const {
  // Special-case Objective-C's for-in loop, which uses the entire loop as its
  // condition. We just print the collection expression.
  const Stmt *Parent = dyn_cast<ObjCForCollectionStmt>(Condition);
  if (!Parent) {
    const ParentMap &Parents = C.getLocationContext()->getParentMap();
    Parent = Parents.getParent(Condition);
  }

  // It is mildly evil to print directly to llvm::outs() rather than emitting
  // warnings, but this ensures things do not get filtered out by the rest of
  // the static analyzer machinery.
  SourceLocation Loc = Parent->getLocStart();
  llvm::outs() << C.getSourceManager().getSpellingLineNumber(Loc) << " "
               << Parent->getStmtClassName() << "\n";
}

void TraversalDumper::checkEndSubprogram(CheckerContext &C) const {
  llvm::outs() << "--END FUNCTION--\n";
}

void ento::registerTraversalDumper(CheckerManager &mgr) {
  mgr.registerChecker<TraversalDumper>();
}

//------------------------------------------------------------------------------

namespace {
class CallDumper : public Checker< check::PreCall > {
public:
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
};
}

void CallDumper::checkPreCall(const CallEvent &Call, CheckerContext &C) const {
  unsigned Indentation = 0;
  for (const LocationContext *LC = C.getLocationContext()->getParent();
       LC != 0; LC = LC->getParent())
    ++Indentation;

  // It is mildly evil to print directly to llvm::outs() rather than emitting
  // warnings, but this ensures things do not get filtered out by the rest of
  // the static analyzer machinery.
  llvm::outs().indent(Indentation);
  Call.dump(llvm::outs());
}

void ento::registerCallDumper(CheckerManager &mgr) {
  mgr.registerChecker<CallDumper>();
}
