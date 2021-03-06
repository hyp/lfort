//===--- CodeGenSubprogram.cpp - Emit LLVM Code from ASTs for a Subprogram ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This coordinates the per-function state used while generating code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenSubprogram.h"
#include "CGCUDARuntime.h"
#include "CGFortranABI.h"
#include "CGDebugInfo.h"
#include "CodeGenModule.h"
#include "lfort/AST/ASTContext.h"
#include "lfort/AST/Decl.h"
#include "lfort/AST/DeclCXX.h"
#include "lfort/AST/StmtCXX.h"
#include "lfort/Basic/TargetInfo.h"
#include "lfort/Frontend/CodeGenOptions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Operator.h"
using namespace lfort;
using namespace CodeGen;

CodeGenSubprogram::CodeGenSubprogram(CodeGenModule &cgm, bool suppressNewContext)
  : CodeGenTypeCache(cgm), CGM(cgm),
    Target(CGM.getContext().getTargetInfo()),
    Builder(cgm.getModule().getContext()),
    SanitizePerformTypeCheck(CGM.getLangOpts().SanitizeNull |
                             CGM.getLangOpts().SanitizeAlignment |
                             CGM.getLangOpts().SanitizeObjectSize |
                             CGM.getLangOpts().SanitizeVptr),
    AutoreleaseResult(false), BlockInfo(0), BlockPointer(0),
    LambdaThisCaptureField(0), NormalCleanupDest(0), NextCleanupDestIndex(1),
    FirstBlockInfo(0), EHResumeBlock(0), ExceptionSlot(0), EHSelectorSlot(0),
    DebugInfo(0), DisableDebugInfo(false), DidCallStackSave(false),
    IndirectBranch(0), SwitchInsn(0), CaseRangeBlock(0), UnreachableBlock(0),
    FortranABIThisDecl(0), FortranABIThisValue(0), CXXThisValue(0), CXXVTTDecl(0),
    CXXVTTValue(0), OutermostConditional(0), TerminateLandingPad(0),
    TerminateHandler(0), TrapBB(0) {
  if (!suppressNewContext)
    CGM.getFortranABI().getMangleContext().startNewSubprogram();

  llvm::FastMathFlags FMF;
  if (CGM.getLangOpts().FastMath)
    FMF.setUnsafeAlgebra();
  if (CGM.getLangOpts().FiniteMathOnly) {
    FMF.setNoNaNs();
    FMF.setNoInfs();
  }
  Builder.SetFastMathFlags(FMF);
}

CodeGenSubprogram::~CodeGenSubprogram() {
  // If there are any unclaimed block infos, go ahead and destroy them
  // now.  This can happen if IR-gen gets clever and skips evaluating
  // something.
  if (FirstBlockInfo)
    destroyBlockInfos(FirstBlockInfo);
}


llvm::Type *CodeGenSubprogram::ConvertTypeForMem(QualType T) {
  return CGM.getTypes().ConvertTypeForMem(T);
}

llvm::Type *CodeGenSubprogram::ConvertType(QualType T) {
  return CGM.getTypes().ConvertType(T);
}

bool CodeGenSubprogram::hasAggregateLLVMType(QualType type) {
  switch (type.getCanonicalType()->getTypeClass()) {
#define TYPE(name, parent)
#define ABSTRACT_TYPE(name, parent)
#define NON_CANONICAL_TYPE(name, parent) case Type::name:
#define DEPENDENT_TYPE(name, parent) case Type::name:
#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(name, parent) case Type::name:
#include "lfort/AST/TypeNodes.def"
    llvm_unreachable("non-canonical or dependent type in IR-generation");

  case Type::Builtin:
  case Type::Pointer:
  case Type::BlockPointer:
  case Type::LValueReference:
  case Type::RValueReference:
  case Type::MemberPointer:
  case Type::Vector:
  case Type::ExtVector:
  case Type::SubprogramProto:
  case Type::SubprogramNoProto:
  case Type::Enum:
  case Type::ObjCObjectPointer:
    return false;

  // Complexes, arrays, records, and Objective-C objects.
  case Type::Complex:
  case Type::ConstantArray:
  case Type::IncompleteArray:
  case Type::VariableArray:
  case Type::Record:
  case Type::ObjCObject:
  case Type::ObjCInterface:
    return true;

  // In IRGen, atomic types are just the underlying type
  case Type::Atomic:
    return hasAggregateLLVMType(type->getAs<AtomicType>()->getValueType());
  }
  llvm_unreachable("unknown type kind!");
}

void CodeGenSubprogram::EmitReturnBlock() {
  // For cleanliness, we try to avoid emitting the return block for
  // simple cases.
  llvm::BasicBlock *CurBB = Builder.GetInsertBlock();

  if (CurBB) {
    assert(!CurBB->getTerminator() && "Unexpected terminated block.");

    // We have a valid insert point, reuse it if it is empty or there are no
    // explicit jumps to the return block.
    if (CurBB->empty() || ReturnBlock.getBlock()->use_empty()) {
      ReturnBlock.getBlock()->replaceAllUsesWith(CurBB);
      delete ReturnBlock.getBlock();
    } else
      EmitBlock(ReturnBlock.getBlock());
    return;
  }

  // Otherwise, if the return block is the target of a single direct
  // branch then we can just put the code in that block instead. This
  // cleans up functions which started with a unified return block.
  if (ReturnBlock.getBlock()->hasOneUse()) {
    llvm::BranchInst *BI =
      dyn_cast<llvm::BranchInst>(*ReturnBlock.getBlock()->use_begin());
    if (BI && BI->isUnconditional() &&
        BI->getSuccessor(0) == ReturnBlock.getBlock()) {
      // Reset insertion point, including debug location, and delete the branch.
      Builder.SetCurrentDebugLocation(BI->getDebugLoc());
      Builder.SetInsertPoint(BI->getParent());
      BI->eraseFromParent();
      delete ReturnBlock.getBlock();
      return;
    }
  }

  // FIXME: We are at an unreachable point, there is no reason to emit the block
  // unless it has uses. However, we still need a place to put the debug
  // region.end for now.

  EmitBlock(ReturnBlock.getBlock());
}

static void EmitIfUsed(CodeGenSubprogram &CGF, llvm::BasicBlock *BB) {
  if (!BB) return;
  if (!BB->use_empty())
    return CGF.CurFn->getBasicBlockList().push_back(BB);
  delete BB;
}

void CodeGenSubprogram::FinishSubprogram(SourceLocation EndLoc) {
  assert(BreakContinueStack.empty() &&
         "mismatched push/pop in break/continue stack!");

  // Pop any cleanups that might have been associated with the
  // parameters.  Do this in whatever block we're currently in; it's
  // important to do this before we enter the return block or return
  // edges will be *really* confused.
  if (EHStack.stable_begin() != PrologueCleanupDepth)
    PopCleanupBlocks(PrologueCleanupDepth);

  // Emit function epilog (to return).
  EmitReturnBlock();

  if (ShouldInstrumentSubprogram())
    EmitSubprogramInstrumentation("__cyg_profile_func_exit");

  // Emit debug descriptor for function end.
  if (CGDebugInfo *DI = getDebugInfo()) {
    DI->setLocation(EndLoc);
    DI->EmitSubprogramEnd(Builder);
  }

  EmitSubprogramEpilog(*CurFnInfo);
  EmitEndEHSpec(CurCodeDecl);

  assert(EHStack.empty() &&
         "did not remove all scopes from cleanup stack!");

  // If someone did an indirect goto, emit the indirect goto block at the end of
  // the function.
  if (IndirectBranch) {
    EmitBlock(IndirectBranch->getParent());
    Builder.ClearInsertionPoint();
  }

  // Remove the AllocaInsertPt instruction, which is just a convenience for us.
  llvm::Instruction *Ptr = AllocaInsertPt;
  AllocaInsertPt = 0;
  Ptr->eraseFromParent();

  // If someone took the address of a label but never did an indirect goto, we
  // made a zero entry PHI node, which is illegal, zap it now.
  if (IndirectBranch) {
    llvm::PHINode *PN = cast<llvm::PHINode>(IndirectBranch->getAddress());
    if (PN->getNumIncomingValues() == 0) {
      PN->replaceAllUsesWith(llvm::UndefValue::get(PN->getType()));
      PN->eraseFromParent();
    }
  }

  EmitIfUsed(*this, EHResumeBlock);
  EmitIfUsed(*this, TerminateLandingPad);
  EmitIfUsed(*this, TerminateHandler);
  EmitIfUsed(*this, UnreachableBlock);

  if (CGM.getCodeGenOpts().EmitDeclMetadata)
    EmitDeclMetadata();
}

/// ShouldInstrumentSubprogram - Return true if the current function should be
/// instrumented with __cyg_profile_func_* calls
bool CodeGenSubprogram::ShouldInstrumentSubprogram() {
  if (!CGM.getCodeGenOpts().InstrumentSubprograms)
    return false;
  if (!CurFuncDecl || CurFuncDecl->hasAttr<NoInstrumentSubprogramAttr>())
    return false;
  return true;
}

/// EmitSubprogramInstrumentation - Emit LLVM code to call the specified
/// instrumentation function with the current function and the call site, if
/// function instrumentation is enabled.
void CodeGenSubprogram::EmitSubprogramInstrumentation(const char *Fn) {
  // void __cyg_profile_func_{enter,exit} (void *this_fn, void *call_site);
  llvm::PointerType *PointerTy = Int8PtrTy;
  llvm::Type *ProfileFuncArgs[] = { PointerTy, PointerTy };
  llvm::FunctionType *SubprogramTy =
    llvm::FunctionType::get(VoidTy, ProfileFuncArgs, false);

  llvm::Constant *F = CGM.CreateRuntimeSubprogram(SubprogramTy, Fn);
  llvm::CallInst *CallSite = Builder.CreateCall(
    CGM.getIntrinsic(llvm::Intrinsic::returnaddress),
    llvm::ConstantInt::get(Int32Ty, 0),
    "callsite");

  Builder.CreateCall2(F,
                      llvm::ConstantExpr::getBitCast(CurFn, PointerTy),
                      CallSite);
}

void CodeGenSubprogram::EmitMCountInstrumentation() {
  llvm::FunctionType *FTy = llvm::FunctionType::get(VoidTy, false);

  llvm::Constant *MCountFn = CGM.CreateRuntimeSubprogram(FTy,
                                                       Target.getMCountName());
  Builder.CreateCall(MCountFn);
}

// OpenCL v1.2 s5.6.4.6 allows the compiler to store kernel argument
// information in the program executable. The argument information stored
// includes the argument name, its type, the address and access qualifiers used.
// FIXME: Add type, address, and access qualifiers.
static void GenOpenCLArgMetadata(const SubprogramDecl *FD, llvm::Function *Fn,
                                 CodeGenModule &CGM,llvm::LLVMContext &Context,
                                 llvm::SmallVector <llvm::Value*, 5> &kernelMDArgs) {

  // Create MDNodes that represents the kernel arg metadata.
  // Each MDNode is a list in the form of "key", N number of values which is
  // the same number of values as their are kernel arguments.

  // MDNode for the kernel argument names.
  SmallVector<llvm::Value*, 8> argNames;
  argNames.push_back(llvm::MDString::get(Context, "kernel_arg_name"));

  for (unsigned i = 0, e = FD->getNumParams(); i != e; ++i) {
    const ParmVarDecl *parm = FD->getParamDecl(i);

    // Get argument name.
    argNames.push_back(llvm::MDString::get(Context, parm->getName()));

  }
  // Add MDNode to the list of all metadata.
  kernelMDArgs.push_back(llvm::MDNode::get(Context, argNames));
}

void CodeGenSubprogram::EmitOpenCLKernelMetadata(const SubprogramDecl *FD,
                                               llvm::Function *Fn)
{
  if (!FD->hasAttr<OpenCLKernelAttr>())
    return;

  llvm::LLVMContext &Context = getLLVMContext();

  llvm::SmallVector <llvm::Value*, 5> kernelMDArgs;
  kernelMDArgs.push_back(Fn);

  if (CGM.getCodeGenOpts().EmitOpenCLArgMetadata)
    GenOpenCLArgMetadata(FD, Fn, CGM, Context, kernelMDArgs);

  if (FD->hasAttr<WorkGroupSizeHintAttr>()) {
    WorkGroupSizeHintAttr *attr = FD->getAttr<WorkGroupSizeHintAttr>();
    llvm::Value *attrMDArgs[] = {
      llvm::MDString::get(Context, "work_group_size_hint"),
      Builder.getInt32(attr->getXDim()),
      Builder.getInt32(attr->getYDim()),
      Builder.getInt32(attr->getZDim())
    };
    kernelMDArgs.push_back(llvm::MDNode::get(Context, attrMDArgs));
  }

  if (FD->hasAttr<ReqdWorkGroupSizeAttr>()) {
    ReqdWorkGroupSizeAttr *attr = FD->getAttr<ReqdWorkGroupSizeAttr>();
    llvm::Value *attrMDArgs[] = {
      llvm::MDString::get(Context, "reqd_work_group_size"),
      Builder.getInt32(attr->getXDim()),
      Builder.getInt32(attr->getYDim()),
      Builder.getInt32(attr->getZDim())
    };
    kernelMDArgs.push_back(llvm::MDNode::get(Context, attrMDArgs));
  }

  llvm::MDNode *kernelMDNode = llvm::MDNode::get(Context, kernelMDArgs);
  llvm::NamedMDNode *OpenCLKernelMetadata =
    CGM.getModule().getOrInsertNamedMetadata("opencl.kernels");
  OpenCLKernelMetadata->addOperand(kernelMDNode);
}

void CodeGenSubprogram::StartSubprogram(GlobalDecl GD, QualType RetTy,
                                    llvm::Function *Fn,
                                    const CGSubprogramInfo &FnInfo,
                                    const SubprogramArgList &Args,
                                    SourceLocation StartLoc) {
  const Decl *D = GD.getDecl();

  DidCallStackSave = false;
  CurCodeDecl = CurFuncDecl = D;
  FnRetTy = RetTy;
  CurFn = Fn;
  CurFnInfo = &FnInfo;
  assert(CurFn->isDeclaration() && "Subprogram already has body?");

  // Pass inline keyword to optimizer if it appears explicitly on any
  // declaration.
  if (!CGM.getCodeGenOpts().NoInline)
    if (const SubprogramDecl *FD = dyn_cast_or_null<SubprogramDecl>(D))
      for (SubprogramDecl::redecl_iterator RI = FD->redecls_begin(),
             RE = FD->redecls_end(); RI != RE; ++RI)
        if (RI->isInlineSpecified()) {
          Fn->addFnAttr(llvm::Attribute::InlineHint);
          break;
        }

  if (getLangOpts().OpenCL) {
    // Add metadata for a kernel function.
    if (const SubprogramDecl *FD = dyn_cast_or_null<SubprogramDecl>(D))
      EmitOpenCLKernelMetadata(FD, Fn);
  }

  llvm::BasicBlock *EntryBB = createBasicBlock("entry", CurFn);

  // Create a marker to make it easy to insert allocas into the entryblock
  // later.  Don't create this with the builder, because we don't want it
  // folded.
  llvm::Value *Undef = llvm::UndefValue::get(Int32Ty);
  AllocaInsertPt = new llvm::BitCastInst(Undef, Int32Ty, "", EntryBB);
  if (Builder.isNamePreserving())
    AllocaInsertPt->setName("allocapt");

  ReturnBlock = getJumpDestInCurrentScope("return");

  Builder.SetInsertPoint(EntryBB);

  // Emit subprogram debug descriptor.
  if (CGDebugInfo *DI = getDebugInfo()) {
    unsigned NumArgs = 0;
    QualType *ArgsArray = new QualType[Args.size()];
    for (SubprogramArgList::const_iterator i = Args.begin(), e = Args.end();
	 i != e; ++i) {
      ArgsArray[NumArgs++] = (*i)->getType();
    }

    QualType FnType =
      getContext().getSubprogramType(RetTy, ArgsArray, NumArgs,
                                   SubprogramProtoType::ExtProtoInfo());

    delete[] ArgsArray;

    DI->setLocation(StartLoc);
    DI->EmitSubprogramStart(GD, FnType, CurFn, Builder);
  }

  if (ShouldInstrumentSubprogram())
    EmitSubprogramInstrumentation("__cyg_profile_func_enter");

  if (CGM.getCodeGenOpts().InstrumentForProfiling)
    EmitMCountInstrumentation();

  if (RetTy->isVoidType()) {
    // Void type; nothing to return.
    ReturnValue = 0;
  } else if (CurFnInfo->getReturnInfo().getKind() == ABIArgInfo::Indirect &&
             hasAggregateLLVMType(CurFnInfo->getReturnType())) {
    // Indirect aggregate return; emit returned value directly into sret slot.
    // This reduces code size, and affects correctness in C++.
    ReturnValue = CurFn->arg_begin();
  } else {
    ReturnValue = CreateIRTemp(RetTy, "retval");

    // Tell the epilog emitter to autorelease the result.  We do this
    // now so that various specialized functions can suppress it
    // during their IR-generation.
    if (getLangOpts().ObjCAutoRefCount &&
        !CurFnInfo->isReturnsRetained() &&
        RetTy->isObjCRetainableType())
      AutoreleaseResult = true;
  }

  EmitStartEHSpec(CurCodeDecl);

  PrologueCleanupDepth = EHStack.stable_begin();
  EmitSubprogramProlog(*CurFnInfo, CurFn, Args);

  if (D && isa<CXXMethodDecl>(D) && cast<CXXMethodDecl>(D)->isInstance()) {
    CGM.getFortranABI().EmitInstanceSubprogramProlog(*this);
    const CXXMethodDecl *MD = cast<CXXMethodDecl>(D);
    if (MD->getParent()->isLambda() &&
        MD->getOverloadedOperator() == OO_Call) {
      // We're in a lambda; figure out the captures.
      MD->getParent()->getCaptureFields(LambdaCaptureFields,
                                        LambdaThisCaptureField);
      if (LambdaThisCaptureField) {
        // If this lambda captures this, load it.
        QualType LambdaTagType =
            getContext().getTagDeclType(LambdaThisCaptureField->getParent());
        LValue LambdaLV = MakeNaturalAlignAddrLValue(FortranABIThisValue,
                                                     LambdaTagType);
        LValue ThisLValue = EmitLValueForField(LambdaLV,
                                               LambdaThisCaptureField);
        CXXThisValue = EmitLoadOfLValue(ThisLValue).getScalarVal();
      }
    } else {
      // Not in a lambda; just use 'this' from the method.
      // FIXME: Should we generate a new load for each use of 'this'?  The
      // fast register allocator would be happier...
      CXXThisValue = FortranABIThisValue;
    }
  }

  // If any of the arguments have a variably modified type, make sure to
  // emit the type size.
  for (SubprogramArgList::const_iterator i = Args.begin(), e = Args.end();
       i != e; ++i) {
    const VarDecl *VD = *i;

    // Dig out the type as written from ParmVarDecls; it's unclear whether
    // the standard (C99 6.9.1p10) requires this, but we're following the
    // precedent set by gcc.
    QualType Ty;
    if (const ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(VD))
      Ty = PVD->getOriginalType();
    else
      Ty = VD->getType();

    if (Ty->isVariablyModifiedType())
      EmitVariablyModifiedType(Ty);
  }
  // Emit a location at the end of the prologue.
  if (CGDebugInfo *DI = getDebugInfo())
    DI->EmitLocation(Builder, StartLoc);
}

void CodeGenSubprogram::EmitSubprogramBody(SubprogramArgList &Args) {
  const SubprogramDecl *FD = cast<SubprogramDecl>(CurGD.getDecl());
  assert(FD->getBody());
  EmitStmt(FD->getBody());
}

/// Tries to mark the given function nounwind based on the
/// non-existence of any throwing calls within it.  We believe this is
/// lightweight enough to do at -O0.
static void TryMarkNoThrow(llvm::Function *F) {
  // LLVM treats 'nounwind' on a function as part of the type, so we
  // can't do this on functions that can be overwritten.
  if (F->mayBeOverridden()) return;

  for (llvm::Function::iterator FI = F->begin(), FE = F->end(); FI != FE; ++FI)
    for (llvm::BasicBlock::iterator
           BI = FI->begin(), BE = FI->end(); BI != BE; ++BI)
      if (llvm::CallInst *Call = dyn_cast<llvm::CallInst>(&*BI)) {
        if (!Call->doesNotThrow())
          return;
      } else if (isa<llvm::ResumeInst>(&*BI)) {
        return;
      }
  F->setDoesNotThrow();
}

void CodeGenSubprogram::GenerateCode(GlobalDecl GD, llvm::Function *Fn,
                                   const CGSubprogramInfo &FnInfo) {
  const SubprogramDecl *FD = cast<SubprogramDecl>(GD.getDecl());

  // Check if we should generate debug info for this function.
  if (!FD->hasAttr<NoDebugAttr>())
    maybeInitializeDebugInfo();

  SubprogramArgList Args;
  QualType ResTy = FD->getResultType();

  CurGD = GD;
  if (isa<CXXMethodDecl>(FD) && cast<CXXMethodDecl>(FD)->isInstance())
    CGM.getFortranABI().BuildInstanceSubprogramParams(*this, ResTy, Args);

  for (unsigned i = 0, e = FD->getNumParams(); i != e; ++i)
    Args.push_back(FD->getParamDecl(i));

  SourceRange BodyRange;
  if (Stmt *Body = FD->getBody()) BodyRange = Body->getSourceRange();

  // Emit the standard function prologue.
  StartSubprogram(GD, ResTy, Fn, FnInfo, Args, BodyRange.getBegin());

  // Generate the body of the function.
  if (isa<CXXDestructorDecl>(FD))
    EmitDestructorBody(Args);
  else if (isa<CXXConstructorDecl>(FD))
    EmitConstructorBody(Args);
  else if (getLangOpts().CUDA &&
           !CGM.getCodeGenOpts().CUDAIsDevice &&
           FD->hasAttr<CUDAGlobalAttr>())
    CGM.getCUDARuntime().EmitDeviceStubBody(*this, Args);
  else if (isa<CXXConversionDecl>(FD) &&
           cast<CXXConversionDecl>(FD)->isLambdaToBlockPointerConversion()) {
    // The lambda conversion to block pointer is special; the semantics can't be
    // expressed in the AST, so IRGen needs to special-case it.
    EmitLambdaToBlockPointerBody(Args);
  } else if (isa<CXXMethodDecl>(FD) &&
             cast<CXXMethodDecl>(FD)->isLambdaStaticInvoker()) {
    // The lambda "__invoke" function is special, because it forwards or
    // clones the body of the function call operator (but is actually static).
    EmitLambdaStaticInvokeSubprogram(cast<CXXMethodDecl>(FD));
  }
  else
    EmitSubprogramBody(Args);

  // C++11 [stmt.return]p2:
  //   Flowing off the end of a function [...] results in undefined behavior in
  //   a value-returning function.
  // C11 6.9.1p12:
  //   If the '}' that terminates a function is reached, and the value of the
  //   function call is used by the caller, the behavior is undefined.
  if (getLangOpts().F90 && !FD->hasImplicitReturnZero() &&
      !FD->getResultType()->isVoidType() && Builder.GetInsertBlock()) {
    if (getLangOpts().SanitizeReturn)
      EmitCheck(Builder.getFalse(), "missing_return",
                EmitCheckSourceLocation(FD->getLocation()),
                llvm::ArrayRef<llvm::Value*>(), CRK_Unrecoverable);
    else if (CGM.getCodeGenOpts().OptimizationLevel == 0)
      Builder.CreateCall(CGM.getIntrinsic(llvm::Intrinsic::trap));
    Builder.CreateUnreachable();
    Builder.ClearInsertionPoint();
  }

  // Emit the standard function epilogue.
  FinishSubprogram(BodyRange.getEnd());

  // If we haven't marked the function nothrow through other means, do
  // a quick pass now to see if we can.
  if (!CurFn->doesNotThrow())
    TryMarkNoThrow(CurFn);
}

/// ContainsLabel - Return true if the statement contains a label in it.  If
/// this statement is not executed normally, it not containing a label means
/// that we can just remove the code.
bool CodeGenSubprogram::ContainsLabel(const Stmt *S, bool IgnoreCaseStmts) {
  // Null statement, not a label!
  if (S == 0) return false;

  // If this is a label, we have to emit the code, consider something like:
  // if (0) {  ...  foo:  bar(); }  goto foo;
  //
  // TODO: If anyone cared, we could track __label__'s, since we know that you
  // can't jump to one from outside their declared region.
  if (isa<LabelStmt>(S))
    return true;

  // If this is a case/default statement, and we haven't seen a switch, we have
  // to emit the code.
  if (isa<SwitchCase>(S) && !IgnoreCaseStmts)
    return true;

  // If this is a switch statement, we want to ignore cases below it.
  if (isa<SwitchStmt>(S))
    IgnoreCaseStmts = true;

  // Scan subexpressions for verboten labels.
  for (Stmt::const_child_range I = S->children(); I; ++I)
    if (ContainsLabel(*I, IgnoreCaseStmts))
      return true;

  return false;
}

/// containsBreak - Return true if the statement contains a break out of it.
/// If the statement (recursively) contains a switch or loop with a break
/// inside of it, this is fine.
bool CodeGenSubprogram::containsBreak(const Stmt *S) {
  // Null statement, not a label!
  if (S == 0) return false;

  // If this is a switch or loop that defines its own break scope, then we can
  // include it and anything inside of it.
  if (isa<SwitchStmt>(S) || isa<WhileStmt>(S) || isa<DoStmt>(S) ||
      isa<ForStmt>(S))
    return false;

  if (isa<BreakStmt>(S))
    return true;

  // Scan subexpressions for verboten breaks.
  for (Stmt::const_child_range I = S->children(); I; ++I)
    if (containsBreak(*I))
      return true;

  return false;
}


/// ConstantFoldsToSimpleInteger - If the specified expression does not fold
/// to a constant, or if it does but contains a label, return false.  If it
/// constant folds return true and set the boolean result in Result.
bool CodeGenSubprogram::ConstantFoldsToSimpleInteger(const Expr *Cond,
                                                   bool &ResultBool) {
  llvm::APSInt ResultInt;
  if (!ConstantFoldsToSimpleInteger(Cond, ResultInt))
    return false;

  ResultBool = ResultInt.getBoolValue();
  return true;
}

/// ConstantFoldsToSimpleInteger - If the specified expression does not fold
/// to a constant, or if it does but contains a label, return false.  If it
/// constant folds return true and set the folded value.
bool CodeGenSubprogram::
ConstantFoldsToSimpleInteger(const Expr *Cond, llvm::APSInt &ResultInt) {
  // FIXME: Rename and handle conversion of other evaluatable things
  // to bool.
  llvm::APSInt Int;
  if (!Cond->EvaluateAsInt(Int, getContext()))
    return false;  // Not foldable, not integer or not fully evaluatable.

  if (CodeGenSubprogram::ContainsLabel(Cond))
    return false;  // Contains a label.

  ResultInt = Int;
  return true;
}



/// EmitBranchOnBoolExpr - Emit a branch on a boolean condition (e.g. for an if
/// statement) to the specified blocks.  Based on the condition, this might try
/// to simplify the codegen of the conditional based on the branch.
///
void CodeGenSubprogram::EmitBranchOnBoolExpr(const Expr *Cond,
                                           llvm::BasicBlock *TrueBlock,
                                           llvm::BasicBlock *FalseBlock) {
  Cond = Cond->IgnoreParens();

  if (const BinaryOperator *CondBOp = dyn_cast<BinaryOperator>(Cond)) {
    // Handle X && Y in a condition.
    if (CondBOp->getOpcode() == BO_LAnd) {
      // If we have "1 && X", simplify the code.  "0 && X" would have constant
      // folded if the case was simple enough.
      bool ConstantBool = false;
      if (ConstantFoldsToSimpleInteger(CondBOp->getLHS(), ConstantBool) &&
          ConstantBool) {
        // br(1 && X) -> br(X).
        return EmitBranchOnBoolExpr(CondBOp->getRHS(), TrueBlock, FalseBlock);
      }

      // If we have "X && 1", simplify the code to use an uncond branch.
      // "X && 0" would have been constant folded to 0.
      if (ConstantFoldsToSimpleInteger(CondBOp->getRHS(), ConstantBool) &&
          ConstantBool) {
        // br(X && 1) -> br(X).
        return EmitBranchOnBoolExpr(CondBOp->getLHS(), TrueBlock, FalseBlock);
      }

      // Emit the LHS as a conditional.  If the LHS conditional is false, we
      // want to jump to the FalseBlock.
      llvm::BasicBlock *LHSTrue = createBasicBlock("land.lhs.true");

      ConditionalEvaluation eval(*this);
      EmitBranchOnBoolExpr(CondBOp->getLHS(), LHSTrue, FalseBlock);
      EmitBlock(LHSTrue);

      // Any temporaries created here are conditional.
      eval.begin(*this);
      EmitBranchOnBoolExpr(CondBOp->getRHS(), TrueBlock, FalseBlock);
      eval.end(*this);

      return;
    }

    if (CondBOp->getOpcode() == BO_LOr) {
      // If we have "0 || X", simplify the code.  "1 || X" would have constant
      // folded if the case was simple enough.
      bool ConstantBool = false;
      if (ConstantFoldsToSimpleInteger(CondBOp->getLHS(), ConstantBool) &&
          !ConstantBool) {
        // br(0 || X) -> br(X).
        return EmitBranchOnBoolExpr(CondBOp->getRHS(), TrueBlock, FalseBlock);
      }

      // If we have "X || 0", simplify the code to use an uncond branch.
      // "X || 1" would have been constant folded to 1.
      if (ConstantFoldsToSimpleInteger(CondBOp->getRHS(), ConstantBool) &&
          !ConstantBool) {
        // br(X || 0) -> br(X).
        return EmitBranchOnBoolExpr(CondBOp->getLHS(), TrueBlock, FalseBlock);
      }

      // Emit the LHS as a conditional.  If the LHS conditional is true, we
      // want to jump to the TrueBlock.
      llvm::BasicBlock *LHSFalse = createBasicBlock("lor.lhs.false");

      ConditionalEvaluation eval(*this);
      EmitBranchOnBoolExpr(CondBOp->getLHS(), TrueBlock, LHSFalse);
      EmitBlock(LHSFalse);

      // Any temporaries created here are conditional.
      eval.begin(*this);
      EmitBranchOnBoolExpr(CondBOp->getRHS(), TrueBlock, FalseBlock);
      eval.end(*this);

      return;
    }
  }

  if (const UnaryOperator *CondUOp = dyn_cast<UnaryOperator>(Cond)) {
    // br(!x, t, f) -> br(x, f, t)
    if (CondUOp->getOpcode() == UO_LNot)
      return EmitBranchOnBoolExpr(CondUOp->getSubExpr(), FalseBlock, TrueBlock);
  }

  if (const ConditionalOperator *CondOp = dyn_cast<ConditionalOperator>(Cond)) {
    // br(c ? x : y, t, f) -> br(c, br(x, t, f), br(y, t, f))
    llvm::BasicBlock *LHSBlock = createBasicBlock("cond.true");
    llvm::BasicBlock *RHSBlock = createBasicBlock("cond.false");

    ConditionalEvaluation cond(*this);
    EmitBranchOnBoolExpr(CondOp->getCond(), LHSBlock, RHSBlock);

    cond.begin(*this);
    EmitBlock(LHSBlock);
    EmitBranchOnBoolExpr(CondOp->getLHS(), TrueBlock, FalseBlock);
    cond.end(*this);

    cond.begin(*this);
    EmitBlock(RHSBlock);
    EmitBranchOnBoolExpr(CondOp->getRHS(), TrueBlock, FalseBlock);
    cond.end(*this);

    return;
  }

  // Emit the code with the fully general case.
  llvm::Value *CondV = EvaluateExprAsBool(Cond);
  Builder.CreateCondBr(CondV, TrueBlock, FalseBlock);
}

/// ErrorUnsupported - Print out an error that codegen doesn't support the
/// specified stmt yet.
void CodeGenSubprogram::ErrorUnsupported(const Stmt *S, const char *Type,
                                       bool OmitOnError) {
  CGM.ErrorUnsupported(S, Type, OmitOnError);
}

/// emitNonZeroVLAInit - Emit the "zero" initialization of a
/// variable-length array whose elements have a non-zero bit-pattern.
///
/// \param baseType the inner-most element type of the array
/// \param src - a char* pointing to the bit-pattern for a single
/// base element of the array
/// \param sizeInChars - the total size of the VLA, in chars
static void emitNonZeroVLAInit(CodeGenSubprogram &CGF, QualType baseType,
                               llvm::Value *dest, llvm::Value *src,
                               llvm::Value *sizeInChars) {
  std::pair<CharUnits,CharUnits> baseSizeAndAlign
    = CGF.getContext().getTypeInfoInChars(baseType);

  CGBuilderTy &Builder = CGF.Builder;

  llvm::Value *baseSizeInChars
    = llvm::ConstantInt::get(CGF.IntPtrTy, baseSizeAndAlign.first.getQuantity());

  llvm::Type *i8p = Builder.getInt8PtrTy();

  llvm::Value *begin = Builder.CreateBitCast(dest, i8p, "vla.begin");
  llvm::Value *end = Builder.CreateInBoundsGEP(dest, sizeInChars, "vla.end");

  llvm::BasicBlock *originBB = CGF.Builder.GetInsertBlock();
  llvm::BasicBlock *loopBB = CGF.createBasicBlock("vla-init.loop");
  llvm::BasicBlock *contBB = CGF.createBasicBlock("vla-init.cont");

  // Make a loop over the VLA.  C99 guarantees that the VLA element
  // count must be nonzero.
  CGF.EmitBlock(loopBB);

  llvm::PHINode *cur = Builder.CreatePHI(i8p, 2, "vla.cur");
  cur->addIncoming(begin, originBB);

  // memcpy the individual element bit-pattern.
  Builder.CreateMemCpy(cur, src, baseSizeInChars,
                       baseSizeAndAlign.second.getQuantity(),
                       /*volatile*/ false);

  // Go to the next element.
  llvm::Value *next = Builder.CreateConstInBoundsGEP1_32(cur, 1, "vla.next");

  // Leave if that's the end of the VLA.
  llvm::Value *done = Builder.CreateICmpEQ(next, end, "vla-init.isdone");
  Builder.CreateCondBr(done, contBB, loopBB);
  cur->addIncoming(next, loopBB);

  CGF.EmitBlock(contBB);
}

void
CodeGenSubprogram::EmitNullInitialization(llvm::Value *DestPtr, QualType Ty) {
  // Ignore empty classes in C++.
  if (getLangOpts().F90) {
    if (const RecordType *RT = Ty->getAs<RecordType>()) {
      if (cast<CXXRecordDecl>(RT->getDecl())->isEmpty())
        return;
    }
  }

  // Cast the dest ptr to the appropriate i8 pointer type.
  unsigned DestAS =
    cast<llvm::PointerType>(DestPtr->getType())->getAddressSpace();
  llvm::Type *BP = Builder.getInt8PtrTy(DestAS);
  if (DestPtr->getType() != BP)
    DestPtr = Builder.CreateBitCast(DestPtr, BP);

  // Get size and alignment info for this aggregate.
  std::pair<CharUnits, CharUnits> TypeInfo =
    getContext().getTypeInfoInChars(Ty);
  CharUnits Size = TypeInfo.first;
  CharUnits Align = TypeInfo.second;

  llvm::Value *SizeVal;
  const VariableArrayType *vla;

  // Don't bother emitting a zero-byte memset.
  if (Size.isZero()) {
    // But note that getTypeInfo returns 0 for a VLA.
    if (const VariableArrayType *vlaType =
          dyn_cast_or_null<VariableArrayType>(
                                          getContext().getAsArrayType(Ty))) {
      QualType eltType;
      llvm::Value *numElts;
      llvm::tie(numElts, eltType) = getVLASize(vlaType);

      SizeVal = numElts;
      CharUnits eltSize = getContext().getTypeSizeInChars(eltType);
      if (!eltSize.isOne())
        SizeVal = Builder.CreateNUWMul(SizeVal, CGM.getSize(eltSize));
      vla = vlaType;
    } else {
      return;
    }
  } else {
    SizeVal = CGM.getSize(Size);
    vla = 0;
  }

  // If the type contains a pointer to data member we can't memset it to zero.
  // Instead, create a null constant and copy it to the destination.
  // TODO: there are other patterns besides zero that we can usefully memset,
  // like -1, which happens to be the pattern used by member-pointers.
  if (!CGM.getTypes().isZeroInitializable(Ty)) {
    // For a VLA, emit a single element, then splat that over the VLA.
    if (vla) Ty = getContext().getBaseElementType(vla);

    llvm::Constant *NullConstant = CGM.EmitNullConstant(Ty);

    llvm::GlobalVariable *NullVariable =
      new llvm::GlobalVariable(CGM.getModule(), NullConstant->getType(),
                               /*isConstant=*/true,
                               llvm::GlobalVariable::PrivateLinkage,
                               NullConstant, Twine());
    llvm::Value *SrcPtr =
      Builder.CreateBitCast(NullVariable, Builder.getInt8PtrTy());

    if (vla) return emitNonZeroVLAInit(*this, Ty, DestPtr, SrcPtr, SizeVal);

    // Get and call the appropriate llvm.memcpy overload.
    Builder.CreateMemCpy(DestPtr, SrcPtr, SizeVal, Align.getQuantity(), false);
    return;
  }

  // Otherwise, just memset the whole thing to zero.  This is legal
  // because in LLVM, all default initializers (other than the ones we just
  // handled above) are guaranteed to have a bit pattern of all zeros.
  Builder.CreateMemSet(DestPtr, Builder.getInt8(0), SizeVal,
                       Align.getQuantity(), false);
}

llvm::BlockAddress *CodeGenSubprogram::GetAddrOfLabel(const LabelDecl *L) {
  // Make sure that there is a block for the indirect goto.
  if (IndirectBranch == 0)
    GetIndirectGotoBlock();

  llvm::BasicBlock *BB = getJumpDestForLabel(L).getBlock();

  // Make sure the indirect branch includes all of the address-taken blocks.
  IndirectBranch->addDestination(BB);
  return llvm::BlockAddress::get(CurFn, BB);
}

llvm::BasicBlock *CodeGenSubprogram::GetIndirectGotoBlock() {
  // If we already made the indirect branch for indirect goto, return its block.
  if (IndirectBranch) return IndirectBranch->getParent();

  CGBuilderTy TmpBuilder(createBasicBlock("indirectgoto"));

  // Create the PHI node that indirect gotos will add entries to.
  llvm::Value *DestVal = TmpBuilder.CreatePHI(Int8PtrTy, 0,
                                              "indirect.goto.dest");

  // Create the indirect branch instruction.
  IndirectBranch = TmpBuilder.CreateIndirectBr(DestVal);
  return IndirectBranch->getParent();
}

/// Computes the length of an array in elements, as well as the base
/// element type and a properly-typed first element pointer.
llvm::Value *CodeGenSubprogram::emitArrayLength(const ArrayType *origArrayType,
                                              QualType &baseType,
                                              llvm::Value *&addr) {
  const ArrayType *arrayType = origArrayType;

  // If it's a VLA, we have to load the stored size.  Note that
  // this is the size of the VLA in bytes, not its size in elements.
  llvm::Value *numVLAElements = 0;
  if (isa<VariableArrayType>(arrayType)) {
    numVLAElements = getVLASize(cast<VariableArrayType>(arrayType)).first;

    // Walk into all VLAs.  This doesn't require changes to addr,
    // which has type T* where T is the first non-VLA element type.
    do {
      QualType elementType = arrayType->getElementType();
      arrayType = getContext().getAsArrayType(elementType);

      // If we only have VLA components, 'addr' requires no adjustment.
      if (!arrayType) {
        baseType = elementType;
        return numVLAElements;
      }
    } while (isa<VariableArrayType>(arrayType));

    // We get out here only if we find a constant array type
    // inside the VLA.
  }

  // We have some number of constant-length arrays, so addr should
  // have LLVM type [M x [N x [...]]]*.  Build a GEP that walks
  // down to the first element of addr.
  SmallVector<llvm::Value*, 8> gepIndices;

  // GEP down to the array type.
  llvm::ConstantInt *zero = Builder.getInt32(0);
  gepIndices.push_back(zero);

  uint64_t countFromCLAs = 1;
  QualType eltType;

  llvm::ArrayType *llvmArrayType =
    dyn_cast<llvm::ArrayType>(
      cast<llvm::PointerType>(addr->getType())->getElementType());
  while (llvmArrayType) {
    assert(isa<ConstantArrayType>(arrayType));
    assert(cast<ConstantArrayType>(arrayType)->getSize().getZExtValue()
             == llvmArrayType->getNumElements());

    gepIndices.push_back(zero);
    countFromCLAs *= llvmArrayType->getNumElements();
    eltType = arrayType->getElementType();

    llvmArrayType =
      dyn_cast<llvm::ArrayType>(llvmArrayType->getElementType());
    arrayType = getContext().getAsArrayType(arrayType->getElementType());
    assert((!llvmArrayType || arrayType) &&
           "LLVM and LFort types are out-of-synch");
  }

  if (arrayType) {
    // From this point onwards, the LFort array type has been emitted
    // as some other type (probably a packed struct). Compute the array
    // size, and just emit the 'begin' expression as a bitcast.
    while (arrayType) {
      countFromCLAs *=
          cast<ConstantArrayType>(arrayType)->getSize().getZExtValue();
      eltType = arrayType->getElementType();
      arrayType = getContext().getAsArrayType(eltType);
    }

    unsigned AddressSpace = addr->getType()->getPointerAddressSpace();
    llvm::Type *BaseType = ConvertType(eltType)->getPointerTo(AddressSpace);
    addr = Builder.CreateBitCast(addr, BaseType, "array.begin");
  } else {
    // Create the actual GEP.
    addr = Builder.CreateInBoundsGEP(addr, gepIndices, "array.begin");
  }

  baseType = eltType;

  llvm::Value *numElements
    = llvm::ConstantInt::get(SizeTy, countFromCLAs);

  // If we had any VLA dimensions, factor them in.
  if (numVLAElements)
    numElements = Builder.CreateNUWMul(numVLAElements, numElements);

  return numElements;
}

std::pair<llvm::Value*, QualType>
CodeGenSubprogram::getVLASize(QualType type) {
  const VariableArrayType *vla = getContext().getAsVariableArrayType(type);
  assert(vla && "type was not a variable array type!");
  return getVLASize(vla);
}

std::pair<llvm::Value*, QualType>
CodeGenSubprogram::getVLASize(const VariableArrayType *type) {
  // The number of elements so far; always size_t.
  llvm::Value *numElements = 0;

  QualType elementType;
  do {
    elementType = type->getElementType();
    llvm::Value *vlaSize = VLASizeMap[type->getSizeExpr()];
    assert(vlaSize && "no size for VLA!");
    assert(vlaSize->getType() == SizeTy);

    if (!numElements) {
      numElements = vlaSize;
    } else {
      // It's undefined behavior if this wraps around, so mark it that way.
      // FIXME: Teach -fcatch-undefined-behavior to trap this.
      numElements = Builder.CreateNUWMul(numElements, vlaSize);
    }
  } while ((type = getContext().getAsVariableArrayType(elementType)));

  return std::pair<llvm::Value*,QualType>(numElements, elementType);
}

void CodeGenSubprogram::EmitVariablyModifiedType(QualType type) {
  assert(type->isVariablyModifiedType() &&
         "Must pass variably modified type to EmitVLASizes!");

  EnsureInsertPoint();

  // We're going to walk down into the type and look for VLA
  // expressions.
  do {
    assert(type->isVariablyModifiedType());

    const Type *ty = type.getTypePtr();
    switch (ty->getTypeClass()) {

#define TYPE(Class, Base)
#define ABSTRACT_TYPE(Class, Base)
#define NON_CANONICAL_TYPE(Class, Base)
#define DEPENDENT_TYPE(Class, Base) case Type::Class:
#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(Class, Base)
#include "lfort/AST/TypeNodes.def"
      llvm_unreachable("unexpected dependent type!");

    // These types are never variably-modified.
    case Type::Builtin:
    case Type::Complex:
    case Type::Vector:
    case Type::ExtVector:
    case Type::Record:
    case Type::Enum:
    case Type::Elaborated:
    case Type::TemplateSpecialization:
    case Type::ObjCObject:
    case Type::ObjCInterface:
    case Type::ObjCObjectPointer:
      llvm_unreachable("type class is never variably-modified!");

    case Type::Pointer:
      type = cast<PointerType>(ty)->getPointeeType();
      break;

    case Type::BlockPointer:
      type = cast<BlockPointerType>(ty)->getPointeeType();
      break;

    case Type::LValueReference:
    case Type::RValueReference:
      type = cast<ReferenceType>(ty)->getPointeeType();
      break;

    case Type::MemberPointer:
      type = cast<MemberPointerType>(ty)->getPointeeType();
      break;

    case Type::ConstantArray:
    case Type::IncompleteArray:
      // Losing element qualification here is fine.
      type = cast<ArrayType>(ty)->getElementType();
      break;

    case Type::VariableArray: {
      // Losing element qualification here is fine.
      const VariableArrayType *vat = cast<VariableArrayType>(ty);

      // Unknown size indication requires no size computation.
      // Otherwise, evaluate and record it.
      if (const Expr *size = vat->getSizeExpr()) {
        // It's possible that we might have emitted this already,
        // e.g. with a typedef and a pointer to it.
        llvm::Value *&entry = VLASizeMap[size];
        if (!entry) {
          llvm::Value *Size = EmitScalarExpr(size);

          // C11 6.7.6.2p5:
          //   If the size is an expression that is not an integer constant
          //   expression [...] each time it is evaluated it shall have a value
          //   greater than zero.
          if (getLangOpts().SanitizeVLABound &&
              size->getType()->isSignedIntegerType()) {
            llvm::Value *Zero = llvm::Constant::getNullValue(Size->getType());
            llvm::Constant *StaticArgs[] = {
              EmitCheckSourceLocation(size->getLocStart()),
              EmitCheckTypeDescriptor(size->getType())
            };
            EmitCheck(Builder.CreateICmpSGT(Size, Zero),
                      "vla_bound_not_positive", StaticArgs, Size,
                      CRK_Recoverable);
          }

          // Always zexting here would be wrong if it weren't
          // undefined behavior to have a negative bound.
          entry = Builder.CreateIntCast(Size, SizeTy, /*signed*/ false);
        }
      }
      type = vat->getElementType();
      break;
    }

    case Type::SubprogramProto:
    case Type::SubprogramNoProto:
      type = cast<SubprogramType>(ty)->getResultType();
      break;

    case Type::Paren:
    case Type::TypeOf:
    case Type::UnaryTransform:
    case Type::Attributed:
    case Type::SubstTemplateTypeParm:
      // Keep walking after single level desugaring.
      type = type.getSingleStepDesugaredType(getContext());
      break;

    case Type::Typedef:
    case Type::Decltype:
    case Type::Auto:
      // Stop walking: nothing to do.
      return;

    case Type::TypeOfExpr:
      // Stop walking: emit typeof expression.
      EmitIgnoredExpr(cast<TypeOfExprType>(ty)->getUnderlyingExpr());
      return;

    case Type::Atomic:
      type = cast<AtomicType>(ty)->getValueType();
      break;
    }
  } while (type->isVariablyModifiedType());
}

llvm::Value* CodeGenSubprogram::EmitVAListRef(const Expr* E) {
  if (getContext().getBuiltinVaListType()->isArrayType())
    return EmitScalarExpr(E);
  return EmitLValue(E).getAddress();
}

void CodeGenSubprogram::EmitDeclRefExprDbgValue(const DeclRefExpr *E,
                                              llvm::Constant *Init) {
  assert (Init && "Invalid DeclRefExpr initializer!");
  if (CGDebugInfo *Dbg = getDebugInfo())
    if (CGM.getCodeGenOpts().getDebugInfo() >= CodeGenOptions::LimitedDebugInfo)
      Dbg->EmitGlobalVariable(E->getDecl(), Init);
}

CodeGenSubprogram::PeepholeProtection
CodeGenSubprogram::protectFromPeepholes(RValue rvalue) {
  // At the moment, the only aggressive peephole we do in IR gen
  // is trunc(zext) folding, but if we add more, we can easily
  // extend this protection.

  if (!rvalue.isScalar()) return PeepholeProtection();
  llvm::Value *value = rvalue.getScalarVal();
  if (!isa<llvm::ZExtInst>(value)) return PeepholeProtection();

  // Just make an extra bitcast.
  assert(HaveInsertPoint());
  llvm::Instruction *inst = new llvm::BitCastInst(value, value->getType(), "",
                                                  Builder.GetInsertBlock());

  PeepholeProtection protection;
  protection.Inst = inst;
  return protection;
}

void CodeGenSubprogram::unprotectFromPeepholes(PeepholeProtection protection) {
  if (!protection.Inst) return;

  // In theory, we could try to duplicate the peepholes now, but whatever.
  protection.Inst->eraseFromParent();
}

llvm::Value *CodeGenSubprogram::EmitAnnotationCall(llvm::Value *AnnotationFn,
                                                 llvm::Value *AnnotatedVal,
                                                 llvm::StringRef AnnotationStr,
                                                 SourceLocation Location) {
  llvm::Value *Args[4] = {
    AnnotatedVal,
    Builder.CreateBitCast(CGM.EmitAnnotationString(AnnotationStr), Int8PtrTy),
    Builder.CreateBitCast(CGM.EmitAnnotationUnit(Location), Int8PtrTy),
    CGM.EmitAnnotationLineNo(Location)
  };
  return Builder.CreateCall(AnnotationFn, Args);
}

void CodeGenSubprogram::EmitVarAnnotations(const VarDecl *D, llvm::Value *V) {
  assert(D->hasAttr<AnnotateAttr>() && "no annotate attribute");
  // FIXME We create a new bitcast for every annotation because that's what
  // llvm-gcc was doing.
  for (specific_attr_iterator<AnnotateAttr>
       ai = D->specific_attr_begin<AnnotateAttr>(),
       ae = D->specific_attr_end<AnnotateAttr>(); ai != ae; ++ai)
    EmitAnnotationCall(CGM.getIntrinsic(llvm::Intrinsic::var_annotation),
                       Builder.CreateBitCast(V, CGM.Int8PtrTy, V->getName()),
                       (*ai)->getAnnotation(), D->getLocation());
}

llvm::Value *CodeGenSubprogram::EmitFieldAnnotations(const FieldDecl *D,
                                                   llvm::Value *V) {
  assert(D->hasAttr<AnnotateAttr>() && "no annotate attribute");
  llvm::Type *VTy = V->getType();
  llvm::Value *F = CGM.getIntrinsic(llvm::Intrinsic::ptr_annotation,
                                    CGM.Int8PtrTy);

  for (specific_attr_iterator<AnnotateAttr>
       ai = D->specific_attr_begin<AnnotateAttr>(),
       ae = D->specific_attr_end<AnnotateAttr>(); ai != ae; ++ai) {
    // FIXME Always emit the cast inst so we can differentiate between
    // annotation on the first field of a struct and annotation on the struct
    // itself.
    if (VTy != CGM.Int8PtrTy)
      V = Builder.Insert(new llvm::BitCastInst(V, CGM.Int8PtrTy));
    V = EmitAnnotationCall(F, V, (*ai)->getAnnotation(), D->getLocation());
    V = Builder.CreateBitCast(V, VTy);
  }

  return V;
}
