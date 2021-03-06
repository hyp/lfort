//===----- CGOpenCLRuntime.h - Interface to OpenCL Runtimes -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for OpenCL code generation.  Concrete
// subclasses of this implement code generation for specific OpenCL
// runtime libraries.
//
//===----------------------------------------------------------------------===//

#ifndef LFORT_CODEGEN_OPENCLRUNTIME_H
#define LFORT_CODEGEN_OPENCLRUNTIME_H

#include "lfort/AST/Type.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

namespace lfort {

class VarDecl;

namespace CodeGen {

class CodeGenSubprogram;
class CodeGenModule;

class CGOpenCLRuntime {
protected:
  CodeGenModule &CGM;

public:
  CGOpenCLRuntime(CodeGenModule &CGM) : CGM(CGM) {}
  virtual ~CGOpenCLRuntime();

  /// Emit the IR required for a work-group-local variable declaration, and add
  /// an entry to CGF's LocalDeclMap for D.  The base class does this using
  /// CodeGenSubprogram::EmitStaticVarDecl to emit an internal global for D.
  virtual void EmitWorkGroupLocalVarDecl(CodeGenSubprogram &CGF,
                                         const VarDecl &D);

  virtual llvm::Type *convertOpenCLSpecificType(const Type *T);
};

}
}

#endif
