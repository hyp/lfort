//===--- GlobalDecl.h - Global declaration holder ---------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// A GlobalDecl can hold either a regular variable/function or a C++ ctor/dtor
// together with its type.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LFORT_AST_GLOBALDECL_H
#define LLVM_LFORT_AST_GLOBALDECL_H

#include "lfort/AST/DeclCXX.h"
#include "lfort/AST/DeclObjC.h"
#include "lfort/Basic/ABI.h"

namespace lfort {

/// GlobalDecl - represents a global declaration. This can either be a
/// CXXConstructorDecl and the constructor type (Base, Complete).
/// a CXXDestructorDecl and the destructor type (Base, Complete) or
/// a VarDecl, a SubprogramDecl or a BlockDecl.
class GlobalDecl {
  llvm::PointerIntPair<const Decl*, 2> Value;

  void Init(const Decl *D) {
    assert(!isa<CXXConstructorDecl>(D) && "Use other ctor with ctor decls!");
    assert(!isa<CXXDestructorDecl>(D) && "Use other ctor with dtor decls!");

    Value.setPointer(D);
  }

public:
  GlobalDecl() {}

  GlobalDecl(const VarDecl *D) { Init(D);}
  GlobalDecl(const SubprogramDecl *D) { Init(D); }
  GlobalDecl(const BlockDecl *D) { Init(D); }
  GlobalDecl(const ObjCMethodDecl *D) { Init(D); }

  GlobalDecl(const CXXConstructorDecl *D, CXXCtorType Type)
  : Value(D, Type) {}
  GlobalDecl(const CXXDestructorDecl *D, CXXDtorType Type)
  : Value(D, Type) {}

  GlobalDecl getCanonicalDecl() const {
    GlobalDecl CanonGD;
    CanonGD.Value.setPointer(Value.getPointer()->getCanonicalDecl());
    CanonGD.Value.setInt(Value.getInt());
    
    return CanonGD;
  }

  const Decl *getDecl() const { return Value.getPointer(); }

  CXXCtorType getCtorType() const {
    assert(isa<CXXConstructorDecl>(getDecl()) && "Decl is not a ctor!");
    return static_cast<CXXCtorType>(Value.getInt());
  }

  CXXDtorType getDtorType() const {
    assert(isa<CXXDestructorDecl>(getDecl()) && "Decl is not a dtor!");
    return static_cast<CXXDtorType>(Value.getInt());
  }
  
  friend bool operator==(const GlobalDecl &LHS, const GlobalDecl &RHS) {
    return LHS.Value == RHS.Value;
  }
  
  void *getAsOpaquePtr() const { return Value.getOpaqueValue(); }

  static GlobalDecl getFromOpaquePtr(void *P) {
    GlobalDecl GD;
    GD.Value.setFromOpaqueValue(P);
    return GD;
  }
  
  GlobalDecl getWithDecl(const Decl *D) {
    GlobalDecl Result(*this);
    Result.Value.setPointer(D);
    return Result;
  }
};

} // end namespace lfort

namespace llvm {
  template<class> struct DenseMapInfo;

  template<> struct DenseMapInfo<lfort::GlobalDecl> {
    static inline lfort::GlobalDecl getEmptyKey() {
      return lfort::GlobalDecl();
    }
  
    static inline lfort::GlobalDecl getTombstoneKey() {
      return lfort::GlobalDecl::
        getFromOpaquePtr(reinterpret_cast<void*>(-1));
    }

    static unsigned getHashValue(lfort::GlobalDecl GD) {
      return DenseMapInfo<void*>::getHashValue(GD.getAsOpaquePtr());
    }
    
    static bool isEqual(lfort::GlobalDecl LHS, 
                        lfort::GlobalDecl RHS) {
      return LHS == RHS;
    }
      
  };
  
  // GlobalDecl isn't *technically* a POD type. However, its copy constructor,
  // copy assignment operator, and destructor are all trivial.
  template <>
  struct isPodLike<lfort::GlobalDecl> {
    static const bool value = true;
  };
} // end namespace llvm

#endif
