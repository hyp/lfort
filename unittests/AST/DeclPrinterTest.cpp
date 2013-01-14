//===- unittests/AST/DeclPrinterTest.cpp --- Declaration printer tests ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains tests for Decl::print() and related methods.
//
// Search this file for WRONG to see test cases that are producing something
// completely wrong, invalid C++ or just misleading.
//
// These tests have a coding convention:
// * declaration to be printed is named 'A' unless it should have some special
// name (e.g., 'operator+');
// * additional helper declarations are 'Z', 'Y', 'X' and so on.
//
//===----------------------------------------------------------------------===//

#include "lfort/AST/ASTContext.h"
#include "lfort/ASTMatchers/ASTMatchFinder.h"
#include "lfort/Tooling/Tooling.h"
#include "llvm/ADT/SmallString.h"
#include "gtest/gtest.h"

using namespace lfort;
using namespace ast_matchers;
using namespace tooling;

namespace {

void PrintDecl(raw_ostream &Out, const ASTContext *Context, const Decl *D) {
  PrintingPolicy Policy = Context->getPrintingPolicy();
  Policy.TerseOutput = true;
  D->print(Out, Policy, /*Indentation*/ 0, /*PrintInstantiation*/ false);
}

class PrintMatch : public MatchFinder::MatchCallback {
  SmallString<1024> Printed;
  unsigned NumFoundDecls;

public:
  PrintMatch() : NumFoundDecls(0) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const Decl *D = Result.Nodes.getDeclAs<Decl>("id");
    if (!D || D->isImplicit())
      return;
    NumFoundDecls++;
    if (NumFoundDecls > 1)
      return;

    llvm::raw_svector_ostream Out(Printed);
    PrintDecl(Out, Result.Context, D);
  }

  StringRef getPrinted() const {
    return Printed;
  }

  unsigned getNumFoundDecls() const {
    return NumFoundDecls;
  }
};

::testing::AssertionResult PrintedDeclMatches(
                                  StringRef Code,
                                  const std::vector<std::string> &Args,
                                  const DeclarationMatcher &NodeMatch,
                                  StringRef ExpectedPrinted,
                                  StringRef FileName) {
  PrintMatch Printer;
  MatchFinder Finder;
  Finder.addMatcher(NodeMatch, &Printer);
  OwningPtr<FrontendActionFactory> Factory(newFrontendActionFactory(&Finder));

  if (!runToolOnCodeWithArgs(Factory->create(), Code, Args, FileName))
    return testing::AssertionFailure() << "Parsing error in \"" << Code << "\"";

  if (Printer.getNumFoundDecls() == 0)
    return testing::AssertionFailure()
        << "Matcher didn't find any declarations";

  if (Printer.getNumFoundDecls() > 1)
    return testing::AssertionFailure()
        << "Matcher should match only one declaration "
           "(found " << Printer.getNumFoundDecls() << ")";

  if (Printer.getPrinted() != ExpectedPrinted)
    return ::testing::AssertionFailure()
      << "Expected \"" << ExpectedPrinted << "\", "
         "got \"" << Printer.getPrinted() << "\"";

  return ::testing::AssertionSuccess();
}

// FIXME: Convert to Fortran where relevant
#if 0

::testing::AssertionResult PrintedDeclCXX98Matches(StringRef Code,
                                                   StringRef DeclName,
                                                   StringRef ExpectedPrinted) {
  std::vector<std::string> Args(1, "-std=c++98");
  return PrintedDeclMatches(Code,
                            Args,
                            namedDecl(hasName(DeclName)).bind("id"),
                            ExpectedPrinted,
                            "input.cc");
}

::testing::AssertionResult PrintedDeclCXX98Matches(
                                  StringRef Code,
                                  const DeclarationMatcher &NodeMatch,
                                  StringRef ExpectedPrinted) {
  std::vector<std::string> Args(1, "-std=c++98");
  return PrintedDeclMatches(Code,
                            Args,
                            NodeMatch,
                            ExpectedPrinted,
                            "input.cc");
}

::testing::AssertionResult PrintedDeclCXX11Matches(StringRef Code,
                                                   StringRef DeclName,
                                                   StringRef ExpectedPrinted) {
  std::vector<std::string> Args(1, "-std=c++11");
  return PrintedDeclMatches(Code,
                            Args,
                            namedDecl(hasName(DeclName)).bind("id"),
                            ExpectedPrinted,
                            "input.cc");
}

::testing::AssertionResult PrintedDeclCXX11Matches(
                                  StringRef Code,
                                  const DeclarationMatcher &NodeMatch,
                                  StringRef ExpectedPrinted) {
  std::vector<std::string> Args(1, "-std=c++11");
  return PrintedDeclMatches(Code,
                            Args,
                            NodeMatch,
                            ExpectedPrinted,
                            "input.cc");
}

::testing::AssertionResult PrintedDeclObjCMatches(
                                  StringRef Code,
                                  const DeclarationMatcher &NodeMatch,
                                  StringRef ExpectedPrinted) {
  std::vector<std::string> Args(1, "");
  return PrintedDeclMatches(Code,
                            Args,
                            NodeMatch,
                            ExpectedPrinted,
                            "input.m");
}

#endif

} // unnamed namespace

#if 0

TEST(DeclPrinter, TestNamespace1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "namespace A { int B; }",
    "A",
    "namespace A {\n}"));
    // Should be: with { ... }
}

TEST(DeclPrinter, TestNamespace2) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "inline namespace A { int B; }",
    "A",
    "inline namespace A {\n}"));
    // Should be: with { ... }
}

TEST(DeclPrinter, TestNamespaceAlias1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "namespace Z { }"
    "namespace A = Z;",
    "A",
    "namespace A = Z"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestNamespaceAlias2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "namespace X { namespace Y {} }"
    "namespace A = X::Y;",
    "A",
    "namespace A = X::Y"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXRecordDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "class A { int a; };",
    "A",
    "class A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestCXXRecordDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A { int a; };",
    "A",
    "struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestCXXRecordDecl3) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "union A { int a; };",
    "A",
    "union A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestCXXRecordDecl4) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "class Z { int a; };"
    "class A : Z { int b; };",
    "A",
    "class A :  Z {\n}"));
    // Should be: with semicolon, with { ... }, without two spaces
}

TEST(DeclPrinter, TestCXXRecordDecl5) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z { int a; };"
    "struct A : Z { int b; };",
    "A",
    "struct A :  Z {\n}"));
    // Should be: with semicolon, with { ... }, without two spaces
}

TEST(DeclPrinter, TestCXXRecordDecl6) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "class Z { int a; };"
    "class A : public Z { int b; };",
    "A",
    "class A : public Z {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestCXXRecordDecl7) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "class Z { int a; };"
    "class A : protected Z { int b; };",
    "A",
    "class A : protected Z {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestCXXRecordDecl8) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "class Z { int a; };"
    "class A : private Z { int b; };",
    "A",
    "class A : private Z {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestCXXRecordDecl9) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "class Z { int a; };"
    "class A : virtual Z { int b; };",
    "A",
    "class A : virtual  Z {\n}"));
    // Should be: with semicolon, with { ... }, without two spaces
}

TEST(DeclPrinter, TestCXXRecordDecl10) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "class Z { int a; };"
    "class A : virtual public Z { int b; };",
    "A",
    "class A : virtual public Z {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestCXXRecordDecl11) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "class Z { int a; };"
    "class Y : virtual public Z { int b; };"
    "class A : virtual public Z, private Y { int c; };",
    "A",
    "class A : virtual public Z, private Y {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestSubprogramDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "void A();",
    "A",
    "void A()"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "void A() {}",
    "A",
    "void A()"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl3) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "void Z();"
    "void A() { Z(); }",
    "A",
    "void A()"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl4) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "extern void A();",
    "A",
    "extern void A()"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl5) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "static void A();",
    "A",
    "static void A()"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl6) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "inline void A();",
    "A",
    "inline void A()"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl7) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "constexpr int A(int a);",
    "A",
    "int A(int a)"));
    // WRONG; Should be: "constexpr int A(int a);"
}

TEST(DeclPrinter, TestSubprogramDecl8) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "void A(int a);",
    "A",
    "void A(int a)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl9) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "void A(...);",
    "A",
    "void A(...)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl10) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "void A(int a, ...);",
    "A",
    "void A(int a, ...)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl11) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "typedef long size_t;"
    "typedef int *pInt;"
    "void A(int a, pInt b, size_t c);",
    "A",
    "void A(int a, pInt b, size_t c)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl12) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "void A(int a, int b = 0);",
    "A",
    "void A(int a, int b = 0)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl13) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "void (*A(int a))(int b);",
    "A",
    "void (*A(int a))(int)"));
    // Should be: with semicolon, with parameter name (?)
}

TEST(DeclPrinter, TestSubprogramDecl14) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T>"
    "void A(T t) { }"
    "template<>"
    "void A(int N) { }",
    functionDecl(hasName("A"), isExplicitTemplateSpecialization()).bind("id"),
    "void A(int N)"));
    // WRONG; Should be: "template <> void A(int N);"));
}


TEST(DeclPrinter, TestCXXConstructorDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A {"
    "  A();"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A()"));
}

TEST(DeclPrinter, TestCXXConstructorDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A {"
    "  A(int a);"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A(int a)"));
}

TEST(DeclPrinter, TestCXXConstructorDecl3) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A {"
    "  A(const A &a);"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A(const A &a)"));
}

TEST(DeclPrinter, TestCXXConstructorDecl4) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A {"
    "  A(const A &a, int = 0);"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A(const A &a, int = 0)"));
}

TEST(DeclPrinter, TestCXXConstructorDecl5) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct A {"
    "  A(const A &&a);"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A(const A &&a)"));
}

TEST(DeclPrinter, TestCXXConstructorDecl6) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A {"
    "  explicit A(int a);"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "explicit A(int a)"));
}

TEST(DeclPrinter, TestCXXConstructorDecl7) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct A {"
    "  constexpr A();"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A()"));
    // WRONG; Should be: "constexpr A();"
}

TEST(DeclPrinter, TestCXXConstructorDecl8) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct A {"
    "  A() = default;"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A() = default"));
}

TEST(DeclPrinter, TestCXXConstructorDecl9) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct A {"
    "  A() = delete;"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A() = delete"));
}

TEST(DeclPrinter, TestCXXConstructorDecl10) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<typename... T>"
    "struct A {"
    "  A(const A &a);"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A<T...>(const A<T...> &a)"));
}

#if !defined(_MSC_VER)
TEST(DeclPrinter, TestCXXConstructorDecl11) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<typename... T>"
    "struct A : public T... {"
    "  A(T&&... ts) : T(ts)... {}"
    "};",
    constructorDecl(ofClass(hasName("A"))).bind("id"),
    "A<T...>(T &&ts...) : T(ts)"));
    // WRONG; Should be: "A(T&&... ts) : T(ts)..."
}
#endif

TEST(DeclPrinter, TestCXXDestructorDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A {"
    "  ~A();"
    "};",
    destructorDecl(ofClass(hasName("A"))).bind("id"),
    "void ~A()"));
    // WRONG; Should be: "~A();"
}

TEST(DeclPrinter, TestCXXDestructorDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A {"
    "  virtual ~A();"
    "};",
    destructorDecl(ofClass(hasName("A"))).bind("id"),
    "virtual void ~A()"));
    // WRONG; Should be: "virtual ~A();"
}

TEST(DeclPrinter, TestCXXConversionDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A {"
    "  operator int();"
    "};",
    methodDecl(ofClass(hasName("A"))).bind("id"),
    "int operator int()"));
    // WRONG; Should be: "operator int();"
}

TEST(DeclPrinter, TestCXXConversionDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct A {"
    "  operator bool();"
    "};",
    methodDecl(ofClass(hasName("A"))).bind("id"),
    "bool operator _Bool()"));
    // WRONG; Should be: "operator bool();"
}

TEST(DeclPrinter, TestCXXConversionDecl3) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {};"
    "struct A {"
    "  operator Z();"
    "};",
    methodDecl(ofClass(hasName("A"))).bind("id"),
    "Z operator struct Z()"));
    // WRONG; Should be: "operator Z();"
}

TEST(DeclPrinter, TestCXXMethodDecl_AllocationSubprogram1) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "namespace std { typedef decltype(sizeof(int)) size_t; }"
    "struct Z {"
    "  void *operator new(std::size_t);"
    "};",
    methodDecl(ofClass(hasName("Z"))).bind("id"),
    "void *operator new(std::size_t)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl_AllocationSubprogram2) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "namespace std { typedef decltype(sizeof(int)) size_t; }"
    "struct Z {"
    "  void *operator new[](std::size_t);"
    "};",
    methodDecl(ofClass(hasName("Z"))).bind("id"),
    "void *operator new[](std::size_t)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl_AllocationSubprogram3) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct Z {"
    "  void operator delete(void *);"
    "};",
    methodDecl(ofClass(hasName("Z"))).bind("id"),
    "void operator delete(void *) noexcept"));
    // Should be: with semicolon, without noexcept?
}

TEST(DeclPrinter, TestCXXMethodDecl_AllocationSubprogram4) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  void operator delete(void *);"
    "};",
    methodDecl(ofClass(hasName("Z"))).bind("id"),
    "void operator delete(void *)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl_AllocationSubprogram5) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct Z {"
    "  void operator delete[](void *);"
    "};",
    methodDecl(ofClass(hasName("Z"))).bind("id"),
    "void operator delete[](void *) noexcept"));
    // Should be: with semicolon, without noexcept?
}

TEST(DeclPrinter, TestCXXMethodDecl_Operator1) {
  const char *OperatorNames[] = {
    "+",  "-",  "*",  "/",  "%",  "^",   "&",   "|",
    "=",  "<",  ">",  "+=", "-=", "*=",  "/=",  "%=",
    "^=", "&=", "|=", "<<", ">>", ">>=", "<<=", "==",  "!=",
    "<=", ">=", "&&", "||",  ",", "->*",
    "()", "[]"
  };

  for (unsigned i = 0, e = llvm::array_lengthof(OperatorNames); i != e; ++i) {
    SmallString<128> Code;
    Code.append("struct Z { void operator");
    Code.append(OperatorNames[i]);
    Code.append("(Z z); };");

    SmallString<128> Expected;
    Expected.append("void operator");
    Expected.append(OperatorNames[i]);
    Expected.append("(Z z)");
    // Should be: with semicolon

    ASSERT_TRUE(PrintedDeclCXX98Matches(
      Code,
      methodDecl(ofClass(hasName("Z"))).bind("id"),
      Expected));
  }
}

TEST(DeclPrinter, TestCXXMethodDecl_Operator2) {
  const char *OperatorNames[] = {
    "~", "!", "++", "--", "->"
  };

  for (unsigned i = 0, e = llvm::array_lengthof(OperatorNames); i != e; ++i) {
    SmallString<128> Code;
    Code.append("struct Z { void operator");
    Code.append(OperatorNames[i]);
    Code.append("(); };");

    SmallString<128> Expected;
    Expected.append("void operator");
    Expected.append(OperatorNames[i]);
    Expected.append("()");
    // Should be: with semicolon

    ASSERT_TRUE(PrintedDeclCXX98Matches(
      Code,
      methodDecl(ofClass(hasName("Z"))).bind("id"),
      Expected));
  }
}

TEST(DeclPrinter, TestCXXMethodDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  void A(int a);"
    "};",
    "A",
    "void A(int a)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  virtual void A(int a);"
    "};",
    "A",
    "virtual void A(int a)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl3) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  virtual void A(int a);"
    "};"
    "struct ZZ : Z {"
    "  void A(int a);"
    "};",
    "ZZ::A",
    "void A(int a)"));
    // Should be: with semicolon
    // TODO: should we print "virtual"?
}

TEST(DeclPrinter, TestCXXMethodDecl4) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  inline void A(int a);"
    "};",
    "A",
    "inline void A(int a)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl5) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  virtual void A(int a) = 0;"
    "};",
    "A",
    "virtual void A(int a) = 0"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl_CVQualifier1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  void A(int a) const;"
    "};",
    "A",
    "void A(int a) const"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl_CVQualifier2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  void A(int a) volatile;"
    "};",
    "A",
    "void A(int a) volatile"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl_CVQualifier3) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  void A(int a) const volatile;"
    "};",
    "A",
    "void A(int a) const volatile"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestCXXMethodDecl_RefQualifier1) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct Z {"
    "  void A(int a) &;"
    "};",
    "A",
    "void A(int a)"));
    // WRONG; Should be: "void A(int a) &;"
}

TEST(DeclPrinter, TestCXXMethodDecl_RefQualifier2) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct Z {"
    "  void A(int a) &&;"
    "};",
    "A",
    "void A(int a)"));
    // WRONG; Should be: "void A(int a) &&;"
}

TEST(DeclPrinter, TestSubprogramDecl_ExceptionSpecification1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  void A(int a) throw();"
    "};",
    "A",
    "void A(int a) throw()"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl_ExceptionSpecification2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z {"
    "  void A(int a) throw(int);"
    "};",
    "A",
    "void A(int a) throw(int)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl_ExceptionSpecification3) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "class ZZ {};"
    "struct Z {"
    "  void A(int a) throw(ZZ, int);"
    "};",
    "A",
    "void A(int a) throw(ZZ, int)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl_ExceptionSpecification4) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct Z {"
    "  void A(int a) noexcept;"
    "};",
    "A",
    "void A(int a) noexcept"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramDecl_ExceptionSpecification5) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct Z {"
    "  void A(int a) noexcept(true);"
    "};",
    "A",
    "void A(int a) noexcept(trueA(int a) noexcept(true)"));
    // WRONG; Should be: "void A(int a) noexcept(true);"
}

TEST(DeclPrinter, TestSubprogramDecl_ExceptionSpecification6) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "struct Z {"
    "  void A(int a) noexcept(1 < 2);"
    "};",
    "A",
    "void A(int a) noexcept(1 < 2A(int a) noexcept(1 < 2)"));
    // WRONG; Should be: "void A(int a) noexcept(1 < 2);"
}

TEST(DeclPrinter, TestSubprogramDecl_ExceptionSpecification7) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<int N>"
    "struct Z {"
    "  void A(int a) noexcept(N < 2);"
    "};",
    "A",
    "void A(int a) noexcept(N < 2A(int a) noexcept(N < 2)"));
    // WRONG; Should be: "void A(int a) noexcept(N < 2);"
}

TEST(DeclPrinter, TestVarDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "char *const (*(*A)[5])(int);",
    "A",
    "char *const (*(*A)[5])(int)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestVarDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "void (*A)() throw(int);",
    "A",
    "void (*A)() throw(int)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestVarDecl3) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "void (*A)() noexcept;",
    "A",
    "void (*A)() noexcept"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestFieldDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T>"
    "struct Z { T A; };",
    "A",
    "T A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestFieldDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<int N>"
    "struct Z { int A[N]; };",
    "A",
    "int A[N]"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestClassTemplateDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T>"
    "struct A { T a; };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <typename T> struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestClassTemplateDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T = int>"
    "struct A { T a; };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <typename T = int> struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestClassTemplateDecl3) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<class T>"
    "struct A { T a; };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <class T> struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestClassTemplateDecl4) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T, typename U>"
    "struct A { T a; U b; };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <typename T, typename U> struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestClassTemplateDecl5) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<int N>"
    "struct A { int a[N]; };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <int N> struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestClassTemplateDecl6) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<int N = 42>"
    "struct A { int a[N]; };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <int N = 42> struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestClassTemplateDecl7) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "typedef int MyInt;"
    "template<MyInt N>"
    "struct A { int a[N]; };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <MyInt N> struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestClassTemplateDecl8) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<template<typename U> class T> struct A { };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <template <typename U> class T> struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestClassTemplateDecl9) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T> struct Z { };"
    "template<template<typename U> class T = Z> struct A { };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <template <typename U> class T> struct A {\n}"));
    // Should be: with semicolon, with { ... }
}

TEST(DeclPrinter, TestClassTemplateDecl10) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<typename... T>"
    "struct A { int a; };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <typename ... T> struct A {\n}"));
    // Should be: with semicolon, with { ... }, without spaces before '...'
}

TEST(DeclPrinter, TestClassTemplateDecl11) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<typename... T>"
    "struct A : public T... { int a; };",
    classTemplateDecl(hasName("A")).bind("id"),
    "template <typename ... T> struct A : public T... {\n}"));
    // Should be: with semicolon, with { ... }, without spaces before '...'
}

TEST(DeclPrinter, TestClassTemplatePartialSpecializationDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T, typename U>"
    "struct A { T a; U b; };"
    "template<typename T>"
    "struct A<T, int> { T a; };",
    classTemplateSpecializationDecl().bind("id"),
    "struct A {\n}"));
    // WRONG; Should be: "template<typename T> struct A<T, int> { ... }"
}

TEST(DeclPrinter, TestClassTemplatePartialSpecializationDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T>"
    "struct A { T a; };"
    "template<typename T>"
    "struct A<T *> { T a; };",
    classTemplateSpecializationDecl().bind("id"),
    "struct A {\n}"));
    // WRONG; Should be: "template<typename T> struct A<T *> { ... }"
}

TEST(DeclPrinter, TestClassTemplateSpecializationDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T>"
    "struct A { T a; };"
    "template<>"
    "struct A<int> { int a; };",
    classTemplateSpecializationDecl().bind("id"),
    "struct A {\n}"));
    // WRONG; Should be: "template<> struct A<int> { ... }"
}

TEST(DeclPrinter, TestSubprogramTemplateDecl1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T>"
    "void A(T &t);",
    functionTemplateDecl(hasName("A")).bind("id"),
    "template <typename T> void A(T &t)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramTemplateDecl2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T>"
    "void A(T &t) { }",
    functionTemplateDecl(hasName("A")).bind("id"),
    "template <typename T> void A(T &t)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramTemplateDecl3) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<typename... T>"
    "void A(T... a);",
    functionTemplateDecl(hasName("A")).bind("id"),
    "template <typename ... T> void A(T a...)"));
    // WRONG; Should be: "template <typename ... T> void A(T... a)"
    //        (not "T a...")
    // Should be: with semicolon.
}

TEST(DeclPrinter, TestSubprogramTemplateDecl4) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z { template<typename T> void A(T t); };",
    functionTemplateDecl(hasName("A")).bind("id"),
    "template <typename T> void A(T t)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramTemplateDecl5) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "struct Z { template<typename T> void A(T t) {} };",
    functionTemplateDecl(hasName("A")).bind("id"),
    "template <typename T> void A(T t)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestSubprogramTemplateDecl6) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T >struct Z {"
    "  template<typename U> void A(U t) {}"
    "};",
    functionTemplateDecl(hasName("A")).bind("id"),
    "template <typename U> void A(U t)"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList1) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T> struct Z {};"
    "struct X {};"
    "Z<X> A;",
    "A",
    "Z<X> A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList2) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T, typename U> struct Z {};"
    "struct X {};"
    "typedef int Y;"
    "Z<X, Y> A;",
    "A",
    "Z<X, Y> A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList3) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T> struct Z {};"
    "template<typename T> struct X {};"
    "Z<X<int> > A;",
    "A",
    "Z<X<int> > A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList4) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<typename T> struct Z {};"
    "template<typename T> struct X {};"
    "Z<X<int>> A;",
    "A",
    "Z<X<int> > A"));
    // Should be: with semicolon, without extra space in "> >"
}

TEST(DeclPrinter, TestTemplateArgumentList5) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T> struct Z {};"
    "template<typename T> struct X { Z<T> A; };",
    "A",
    "Z<T> A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList6) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<template<typename T> class U> struct Z {};"
    "template<typename T> struct X {};"
    "Z<X> A;",
    "A",
    "Z<X> A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList7) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<template<typename T> class U> struct Z {};"
    "template<template<typename T> class U> struct Y {"
    "  Z<U> A;"
    "};",
    "A",
    "Z<U> A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList8) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<typename T> struct Z {};"
    "template<template<typename T> class U> struct Y {"
    "  Z<U<int> > A;"
    "};",
    "A",
    "Z<U<int> > A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList9) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<unsigned I> struct Z {};"
    "Z<0> A;",
    "A",
    "Z<0> A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList10) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<unsigned I> struct Z {};"
    "template<unsigned I> struct X { Z<I> A; };",
    "A",
    "Z<I> A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList11) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<int I> struct Z {};"
    "Z<42 * 10 - 420 / 1> A;",
    "A",
    "Z<42 * 10 - 420 / 1> A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList12) {
  ASSERT_TRUE(PrintedDeclCXX98Matches(
    "template<const char *p> struct Z {};"
    "extern const char X[] = \"aaa\";"
    "Z<X> A;",
    "A",
    "Z<X> A"));
    // Should be: with semicolon
}

TEST(DeclPrinter, TestTemplateArgumentList13) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<typename... T> struct Z {};"
    "template<typename... T> struct X {"
    "  Z<T...> A;"
    "};",
    "A",
    "Z<T...> A"));
    // Should be: with semicolon, without extra space in "> >"
}

TEST(DeclPrinter, TestTemplateArgumentList14) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<typename... T> struct Z {};"
    "template<typename T> struct Y {};"
    "template<typename... T> struct X {"
    "  Z<Y<T>...> A;"
    "};",
    "A",
    "Z<Y<T>...> A"));
    // Should be: with semicolon, without extra space in "> >"
}

TEST(DeclPrinter, TestTemplateArgumentList15) {
  ASSERT_TRUE(PrintedDeclCXX11Matches(
    "template<unsigned I> struct Z {};"
    "template<typename... T> struct X {"
    "  Z<sizeof...(T)> A;"
    "};",
    "A",
    "Z<sizeof...(T)> A"));
    // Should be: with semicolon, without extra space in "> >"
}

TEST(DeclPrinter, TestObjCMethod1) {
  ASSERT_TRUE(PrintedDeclObjCMatches(
    "__attribute__((objc_root_class)) @interface X\n"
    "- (int)A:(id)anObject inRange:(long)range;\n"
    "@end\n"
    "@implementation X\n"
    "- (int)A:(id)anObject inRange:(long)range { int printThis; return 0; }\n"
    "@end\n",
    namedDecl(hasName("A:inRange:"),
              hasDescendant(namedDecl(hasName("printThis")))).bind("id"),
    "- (int) A:(id)anObject inRange:(long)range"));
}

TEST(DeclPrinter, TestObjCProtocol1) {
  ASSERT_TRUE(PrintedDeclObjCMatches(
    "@protocol P1, P2;",
    namedDecl(hasName("P1")).bind("id"),
    "@protocol P1;\n"));
  ASSERT_TRUE(PrintedDeclObjCMatches(
    "@protocol P1, P2;",
    namedDecl(hasName("P2")).bind("id"),
    "@protocol P2;\n"));
}

TEST(DeclPrinter, TestObjCProtocol2) {
  ASSERT_TRUE(PrintedDeclObjCMatches(
    "@protocol P2 @end"
    "@protocol P1<P2> @end",
    namedDecl(hasName("P1")).bind("id"),
    "@protocol P1<P2>\n@end"));
}

#endif

