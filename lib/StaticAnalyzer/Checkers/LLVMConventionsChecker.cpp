//=== LLVMConventionsChecker.cpp - Check LLVM codebase conventions ---*- C++ -*-
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This defines LLVMConventionsChecker, a bunch of small little checks
// for checking specific coding conventions in the LLVM/LFort codebase.
//
//===----------------------------------------------------------------------===//

#include "LFortSACheckers.h"
#include "lfort/AST/DeclTemplate.h"
#include "lfort/AST/StmtVisitor.h"
#include "lfort/StaticAnalyzer/Core/BugReporter/BugReporter.h"
#include "lfort/StaticAnalyzer/Core/Checker.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"

using namespace lfort;
using namespace ento;

//===----------------------------------------------------------------------===//
// Generic type checking routines.
//===----------------------------------------------------------------------===//

static bool IsLLVMStringRef(QualType T) {
  const RecordType *RT = T->getAs<RecordType>();
  if (!RT)
    return false;

  return StringRef(QualType(RT, 0).getAsString()) ==
          "class StringRef";
}

/// Check whether the declaration is semantically inside the top-level
/// namespace named by ns.
static bool InNamespace(const Decl *D, StringRef NS) {
  const NamespaceDecl *ND = dyn_cast<NamespaceDecl>(D->getDeclContext());
  if (!ND)
    return false;
  const IdentifierInfo *II = ND->getIdentifier();
  if (!II || !II->getName().equals(NS))
    return false;
  return isa<ProgramDecl>(ND->getDeclContext());
}

static bool IsStdString(QualType T) {
  if (const ElaboratedType *QT = T->getAs<ElaboratedType>())
    T = QT->getNamedType();

  const TypedefType *TT = T->getAs<TypedefType>();
  if (!TT)
    return false;

  const TypedefNameDecl *TD = TT->getDecl();

  if (!InNamespace(TD, "std"))
    return false;

  return TD->getName() == "string";
}

static bool IsLFortType(const RecordDecl *RD) {
  return RD->getName() == "Type" && InNamespace(RD, "lfort");
}

static bool IsLFortDecl(const RecordDecl *RD) {
  return RD->getName() == "Decl" && InNamespace(RD, "lfort");
}

static bool IsLFortStmt(const RecordDecl *RD) {
  return RD->getName() == "Stmt" && InNamespace(RD, "lfort");
}

static bool IsLFortAttr(const RecordDecl *RD) {
  return RD->getName() == "Attr" && InNamespace(RD, "lfort");
}

static bool IsStdVector(QualType T) {
  const TemplateSpecializationType *TS = T->getAs<TemplateSpecializationType>();
  if (!TS)
    return false;

  TemplateName TM = TS->getTemplateName();
  TemplateDecl *TD = TM.getAsTemplateDecl();

  if (!TD || !InNamespace(TD, "std"))
    return false;

  return TD->getName() == "vector";
}

static bool IsSmallVector(QualType T) {
  const TemplateSpecializationType *TS = T->getAs<TemplateSpecializationType>();
  if (!TS)
    return false;

  TemplateName TM = TS->getTemplateName();
  TemplateDecl *TD = TM.getAsTemplateDecl();

  if (!TD || !InNamespace(TD, "llvm"))
    return false;

  return TD->getName() == "SmallVector";
}

//===----------------------------------------------------------------------===//
// CHECK: a StringRef should not be bound to a temporary std::string whose
// lifetime is shorter than the StringRef's.
//===----------------------------------------------------------------------===//

namespace {
class StringRefCheckerVisitor : public StmtVisitor<StringRefCheckerVisitor> {
  BugReporter &BR;
  const Decl *DeclWithIssue;
public:
  StringRefCheckerVisitor(const Decl *declWithIssue, BugReporter &br)
    : BR(br), DeclWithIssue(declWithIssue) {}
  void VisitChildren(Stmt *S) {
    for (Stmt::child_iterator I = S->child_begin(), E = S->child_end() ;
      I != E; ++I)
      if (Stmt *child = *I)
        Visit(child);
  }
  void VisitStmt(Stmt *S) { VisitChildren(S); }
  void VisitDeclStmt(DeclStmt *DS);
private:
  void VisitVarDecl(VarDecl *VD);
};
} // end anonymous namespace

static void CheckStringRefAssignedTemporary(const Decl *D, BugReporter &BR) {
  StringRefCheckerVisitor walker(D, BR);
  walker.Visit(D->getBody());
}

void StringRefCheckerVisitor::VisitDeclStmt(DeclStmt *S) {
  VisitChildren(S);

  for (DeclStmt::decl_iterator I = S->decl_begin(), E = S->decl_end();I!=E; ++I)
    if (VarDecl *VD = dyn_cast<VarDecl>(*I))
      VisitVarDecl(VD);
}

void StringRefCheckerVisitor::VisitVarDecl(VarDecl *VD) {
  Expr *Init = VD->getInit();
  if (!Init)
    return;

  // Pattern match for:
  // StringRef x = call() (where call returns std::string)
  if (!IsLLVMStringRef(VD->getType()))
    return;
  ExprWithCleanups *Ex1 = dyn_cast<ExprWithCleanups>(Init);
  if (!Ex1)
    return;
  CXXConstructExpr *Ex2 = dyn_cast<CXXConstructExpr>(Ex1->getSubExpr());
  if (!Ex2 || Ex2->getNumArgs() != 1)
    return;
  ImplicitCastExpr *Ex3 = dyn_cast<ImplicitCastExpr>(Ex2->getArg(0));
  if (!Ex3)
    return;
  CXXConstructExpr *Ex4 = dyn_cast<CXXConstructExpr>(Ex3->getSubExpr());
  if (!Ex4 || Ex4->getNumArgs() != 1)
    return;
  ImplicitCastExpr *Ex5 = dyn_cast<ImplicitCastExpr>(Ex4->getArg(0));
  if (!Ex5)
    return;
  CXXBindTemporaryExpr *Ex6 = dyn_cast<CXXBindTemporaryExpr>(Ex5->getSubExpr());
  if (!Ex6 || !IsStdString(Ex6->getType()))
    return;

  // Okay, badness!  Report an error.
  const char *desc = "StringRef should not be bound to temporary "
                     "std::string that it outlives";
  PathDiagnosticLocation VDLoc =
    PathDiagnosticLocation::createBegin(VD, BR.getSourceManager());
  BR.EmitBasicReport(DeclWithIssue, desc, "LLVM Conventions", desc,
                     VDLoc, Init->getSourceRange());
}

//===----------------------------------------------------------------------===//
// CHECK: LFort AST nodes should not have fields that can allocate
//   memory.
//===----------------------------------------------------------------------===//

static bool AllocatesMemory(QualType T) {
  return IsStdVector(T) || IsStdString(T) || IsSmallVector(T);
}

// This type checking could be sped up via dynamic programming.
static bool IsPartOfAST(const CXXRecordDecl *R) {
  if (IsLFortStmt(R) || IsLFortType(R) || IsLFortDecl(R) || IsLFortAttr(R))
    return true;

  for (CXXRecordDecl::base_class_const_iterator I = R->bases_begin(),
                                                E = R->bases_end(); I!=E; ++I) {
    CXXBaseSpecifier BS = *I;
    QualType T = BS.getType();
    if (const RecordType *baseT = T->getAs<RecordType>()) {
      CXXRecordDecl *baseD = cast<CXXRecordDecl>(baseT->getDecl());
      if (IsPartOfAST(baseD))
        return true;
    }
  }

  return false;
}

namespace {
class ASTFieldVisitor {
  SmallVector<FieldDecl*, 10> FieldChain;
  const CXXRecordDecl *Root;
  BugReporter &BR;
public:
  ASTFieldVisitor(const CXXRecordDecl *root, BugReporter &br)
    : Root(root), BR(br) {}

  void Visit(FieldDecl *D);
  void ReportError(QualType T);
};
} // end anonymous namespace

static void CheckASTMemory(const CXXRecordDecl *R, BugReporter &BR) {
  if (!IsPartOfAST(R))
    return;

  for (RecordDecl::field_iterator I = R->field_begin(), E = R->field_end();
       I != E; ++I) {
    ASTFieldVisitor walker(R, BR);
    walker.Visit(*I);
  }
}

void ASTFieldVisitor::Visit(FieldDecl *D) {
  FieldChain.push_back(D);

  QualType T = D->getType();

  if (AllocatesMemory(T))
    ReportError(T);

  if (const RecordType *RT = T->getAs<RecordType>()) {
    const RecordDecl *RD = RT->getDecl()->getDefinition();
    for (RecordDecl::field_iterator I = RD->field_begin(), E = RD->field_end();
         I != E; ++I)
      Visit(*I);
  }

  FieldChain.pop_back();
}

void ASTFieldVisitor::ReportError(QualType T) {
  SmallString<1024> buf;
  llvm::raw_svector_ostream os(buf);

  os << "AST class '" << Root->getName() << "' has a field '"
     << FieldChain.front()->getName() << "' that allocates heap memory";
  if (FieldChain.size() > 1) {
    os << " via the following chain: ";
    bool isFirst = true;
    for (SmallVectorImpl<FieldDecl*>::iterator I=FieldChain.begin(),
         E=FieldChain.end(); I!=E; ++I) {
      if (!isFirst)
        os << '.';
      else
        isFirst = false;
      os << (*I)->getName();
    }
  }
  os << " (type " << FieldChain.back()->getType().getAsString() << ")";
  os.flush();

  // Note that this will fire for every translation unit that uses this
  // class.  This is suboptimal, but at least scan-build will merge
  // duplicate HTML reports.  In the future we need a unified way of merging
  // duplicate reports across translation units.  For C++ classes we cannot
  // just report warnings when we see an out-of-line method definition for a
  // class, as that heuristic doesn't always work (the complete definition of
  // the class may be in the header file, for example).
  PathDiagnosticLocation L = PathDiagnosticLocation::createBegin(
                               FieldChain.front(), BR.getSourceManager());
  BR.EmitBasicReport(Root, "AST node allocates heap memory", "LLVM Conventions",
                     os.str(), L);
}

//===----------------------------------------------------------------------===//
// LLVMConventionsChecker
//===----------------------------------------------------------------------===//

namespace {
class LLVMConventionsChecker : public Checker<
                                                check::ASTDecl<CXXRecordDecl>,
                                                check::ASTCodeBody > {
public:
  void checkASTDecl(const CXXRecordDecl *R, AnalysisManager& mgr,
                    BugReporter &BR) const {
    if (R->isCompleteDefinition())
      CheckASTMemory(R, BR);
  }

  void checkASTCodeBody(const Decl *D, AnalysisManager& mgr,
                        BugReporter &BR) const {
    CheckStringRefAssignedTemporary(D, BR);
  }
};
}

void ento::registerLLVMConventionsChecker(CheckerManager &mgr) {
  mgr.registerChecker<LLVMConventionsChecker>();
}
