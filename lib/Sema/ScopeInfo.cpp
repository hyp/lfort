//===--- ScopeInfo.cpp - Information about a semantic context -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements SubprogramScopeInfo and its subclasses, which contain
// information about a single function, block, lambda, or method body.
//
//===----------------------------------------------------------------------===//

#include "lfort/Sema/ScopeInfo.h"
#include "lfort/AST/Decl.h"
#include "lfort/AST/DeclObjC.h"
#include "lfort/AST/Expr.h"
#include "lfort/AST/ExprCXX.h"
#include "lfort/AST/ExprObjC.h"

using namespace lfort;
using namespace sema;

void SubprogramScopeInfo::Clear() {
  HasBranchProtectedScope = false;
  HasBranchIntoScope = false;
  HasIndirectGoto = false;

  SwitchStack.clear();
  Returns.clear();
  ErrorTrap.reset();
  PossiblyUnreachableDiags.clear();
  WeakObjectUses.clear();
}

static const NamedDecl *getBestPropertyDecl(const ObjCPropertyRefExpr *PropE) {
  if (PropE->isExplicitProperty())
    return PropE->getExplicitProperty();

  return PropE->getImplicitPropertyGetter();
}

SubprogramScopeInfo::WeakObjectProfileTy::BaseInfoTy
SubprogramScopeInfo::WeakObjectProfileTy::getBaseInfo(const Expr *E) {
  E = E->IgnoreParenCasts();

  const NamedDecl *D = 0;
  bool IsExact = false;

  switch (E->getStmtClass()) {
  case Stmt::DeclRefExprClass:
    D = cast<DeclRefExpr>(E)->getDecl();
    IsExact = isa<VarDecl>(D);
    break;
  case Stmt::MemberExprClass: {
    const MemberExpr *ME = cast<MemberExpr>(E);
    D = ME->getMemberDecl();
    IsExact = isa<CXXThisExpr>(ME->getBase()->IgnoreParenImpCasts());
    break;
  }
  case Stmt::ObjCIvarRefExprClass: {
    const ObjCIvarRefExpr *IE = cast<ObjCIvarRefExpr>(E);
    D = IE->getDecl();
    IsExact = IE->getBase()->isObjCSelfExpr();
    break;
  }
  case Stmt::PseudoObjectExprClass: {
    const PseudoObjectExpr *POE = cast<PseudoObjectExpr>(E);
    const ObjCPropertyRefExpr *BaseProp =
      dyn_cast<ObjCPropertyRefExpr>(POE->getSyntacticForm());
    if (BaseProp) {
      D = getBestPropertyDecl(BaseProp);

      const Expr *DoubleBase = BaseProp->getBase();
      if (const OpaqueValueExpr *OVE = dyn_cast<OpaqueValueExpr>(DoubleBase))
        DoubleBase = OVE->getSourceExpr();

      IsExact = DoubleBase->isObjCSelfExpr();
    }
    break;
  }
  default:
    break;
  }

  return BaseInfoTy(D, IsExact);
}


SubprogramScopeInfo::WeakObjectProfileTy::WeakObjectProfileTy(
                                          const ObjCPropertyRefExpr *PropE)
    : Base(0, true), Property(getBestPropertyDecl(PropE)) {

  if (PropE->isObjectReceiver()) {
    const OpaqueValueExpr *OVE = cast<OpaqueValueExpr>(PropE->getBase());
    const Expr *E = OVE->getSourceExpr();
    Base = getBaseInfo(E);
  } else if (PropE->isClassReceiver()) {
    Base.setPointer(PropE->getClassReceiver());
  } else {
    assert(PropE->isSuperReceiver());
  }
}

SubprogramScopeInfo::WeakObjectProfileTy::WeakObjectProfileTy(const Expr *BaseE,
                                                const ObjCPropertyDecl *Prop)
    : Base(0, true), Property(Prop) {
  if (BaseE)
    Base = getBaseInfo(BaseE);
  // else, this is a message accessing a property on super.
}

SubprogramScopeInfo::WeakObjectProfileTy::WeakObjectProfileTy(
                                                      const DeclRefExpr *DRE)
  : Base(0, true), Property(DRE->getDecl()) {
  assert(isa<VarDecl>(Property));
}

SubprogramScopeInfo::WeakObjectProfileTy::WeakObjectProfileTy(
                                                  const ObjCIvarRefExpr *IvarE)
  : Base(getBaseInfo(IvarE->getBase())), Property(IvarE->getDecl()) {
}

void SubprogramScopeInfo::recordUseOfWeak(const ObjCMessageExpr *Msg,
                                        const ObjCPropertyDecl *Prop) {
  assert(Msg && Prop);
  WeakUseVector &Uses =
    WeakObjectUses[WeakObjectProfileTy(Msg->getInstanceReceiver(), Prop)];
  Uses.push_back(WeakUseTy(Msg, Msg->getNumArgs() == 0));
}

void SubprogramScopeInfo::markSafeWeakUse(const Expr *E) {
  E = E->IgnoreParenCasts();

  if (const PseudoObjectExpr *POE = dyn_cast<PseudoObjectExpr>(E)) {
    markSafeWeakUse(POE->getSyntacticForm());
    return;
  }

  if (const ConditionalOperator *Cond = dyn_cast<ConditionalOperator>(E)) {
    markSafeWeakUse(Cond->getTrueExpr());
    markSafeWeakUse(Cond->getFalseExpr());
    return;
  }

  if (const BinaryConditionalOperator *Cond =
        dyn_cast<BinaryConditionalOperator>(E)) {
    markSafeWeakUse(Cond->getCommon());
    markSafeWeakUse(Cond->getFalseExpr());
    return;
  }

  // Has this weak object been seen before?
  SubprogramScopeInfo::WeakObjectUseMap::iterator Uses;
  if (const ObjCPropertyRefExpr *RefExpr = dyn_cast<ObjCPropertyRefExpr>(E))
    Uses = WeakObjectUses.find(WeakObjectProfileTy(RefExpr));
  else if (const ObjCIvarRefExpr *IvarE = dyn_cast<ObjCIvarRefExpr>(E))
    Uses = WeakObjectUses.find(WeakObjectProfileTy(IvarE));
  else if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E))
    Uses = WeakObjectUses.find(WeakObjectProfileTy(DRE));
  else if (const ObjCMessageExpr *MsgE = dyn_cast<ObjCMessageExpr>(E)) {
    Uses = WeakObjectUses.end();
    if (const ObjCMethodDecl *MD = MsgE->getMethodDecl()) {
      if (const ObjCPropertyDecl *Prop = MD->findPropertyDecl()) {
        Uses =
          WeakObjectUses.find(WeakObjectProfileTy(MsgE->getInstanceReceiver(),
                                                  Prop));
      }
    }
  }
  else
    return;

  if (Uses == WeakObjectUses.end())
    return;

  // Has there been a read from the object using this Expr?
  SubprogramScopeInfo::WeakUseVector::reverse_iterator ThisUse =
    std::find(Uses->second.rbegin(), Uses->second.rend(), WeakUseTy(E, true));
  if (ThisUse == Uses->second.rend())
    return;

  ThisUse->markSafe();
}

SubprogramScopeInfo::~SubprogramScopeInfo() { }
BlockScopeInfo::~BlockScopeInfo() { }
LambdaScopeInfo::~LambdaScopeInfo() { }
