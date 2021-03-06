//===- unittest/Tooling/RefactoringTest.cpp - Refactoring unit tests ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "RewriterTestContext.h"
#include "lfort/AST/ASTConsumer.h"
#include "lfort/AST/ASTContext.h"
#include "lfort/AST/DeclCXX.h"
#include "lfort/AST/DeclGroup.h"
#include "lfort/AST/RecursiveASTVisitor.h"
#include "lfort/Basic/Diagnostic.h"
#include "lfort/Basic/DiagnosticOptions.h"
#include "lfort/Basic/FileManager.h"
#include "lfort/Basic/LangOptions.h"
#include "lfort/Basic/SourceManager.h"
#include "lfort/Frontend/CompilerInstance.h"
#include "lfort/Frontend/FrontendAction.h"
#include "lfort/Frontend/TextDiagnosticPrinter.h"
#include "lfort/Rewrite/Core/Rewriter.h"
#include "lfort/Tooling/Refactoring.h"
#include "lfort/Tooling/Tooling.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Path.h"
#include "gtest/gtest.h"

namespace lfort {
namespace tooling {

class ReplacementTest : public ::testing::Test {
 protected:
  Replacement createReplacement(SourceLocation Start, unsigned Length,
                                llvm::StringRef ReplacementText) {
    return Replacement(Context.Sources, Start, Length, ReplacementText);
  }

  RewriterTestContext Context;
};

TEST_F(ReplacementTest, CanDeleteAllText) {
  FileID ID = Context.createInMemoryFile("input.cpp", "text");
  SourceLocation Location = Context.getLocation(ID, 1, 1);
  Replacement Replace(createReplacement(Location, 4, ""));
  EXPECT_TRUE(Replace.apply(Context.Rewrite));
  EXPECT_EQ("", Context.getRewrittenText(ID));
}

TEST_F(ReplacementTest, CanDeleteAllTextInTextWithNewlines) {
  FileID ID = Context.createInMemoryFile("input.cpp", "line1\nline2\nline3");
  SourceLocation Location = Context.getLocation(ID, 1, 1);
  Replacement Replace(createReplacement(Location, 17, ""));
  EXPECT_TRUE(Replace.apply(Context.Rewrite));
  EXPECT_EQ("", Context.getRewrittenText(ID));
}

TEST_F(ReplacementTest, CanAddText) {
  FileID ID = Context.createInMemoryFile("input.cpp", "");
  SourceLocation Location = Context.getLocation(ID, 1, 1);
  Replacement Replace(createReplacement(Location, 0, "result"));
  EXPECT_TRUE(Replace.apply(Context.Rewrite));
  EXPECT_EQ("result", Context.getRewrittenText(ID));
}

TEST_F(ReplacementTest, CanReplaceTextAtPosition) {
  FileID ID = Context.createInMemoryFile("input.cpp",
                                         "line1\nline2\nline3\nline4");
  SourceLocation Location = Context.getLocation(ID, 2, 3);
  Replacement Replace(createReplacement(Location, 12, "x"));
  EXPECT_TRUE(Replace.apply(Context.Rewrite));
  EXPECT_EQ("line1\nlixne4", Context.getRewrittenText(ID));
}

TEST_F(ReplacementTest, CanReplaceTextAtPositionMultipleTimes) {
  FileID ID = Context.createInMemoryFile("input.cpp",
                                         "line1\nline2\nline3\nline4");
  SourceLocation Location1 = Context.getLocation(ID, 2, 3);
  Replacement Replace1(createReplacement(Location1, 12, "x\ny\n"));
  EXPECT_TRUE(Replace1.apply(Context.Rewrite));
  EXPECT_EQ("line1\nlix\ny\nne4", Context.getRewrittenText(ID));

  // Since the original source has not been modified, the (4, 4) points to the
  // 'e' in the original content.
  SourceLocation Location2 = Context.getLocation(ID, 4, 4);
  Replacement Replace2(createReplacement(Location2, 1, "f"));
  EXPECT_TRUE(Replace2.apply(Context.Rewrite));
  EXPECT_EQ("line1\nlix\ny\nnf4", Context.getRewrittenText(ID));
}

TEST_F(ReplacementTest, ApplyFailsForNonExistentLocation) {
  Replacement Replace("nonexistent-file.cpp", 0, 1, "");
  EXPECT_FALSE(Replace.apply(Context.Rewrite));
}

TEST_F(ReplacementTest, CanRetrivePath) {
  Replacement Replace("/path/to/file.cpp", 0, 1, "");
  EXPECT_EQ("/path/to/file.cpp", Replace.getFilePath());
}

TEST_F(ReplacementTest, ReturnsInvalidPath) {
  Replacement Replace1(Context.Sources, SourceLocation(), 0, "");
  EXPECT_TRUE(Replace1.getFilePath().empty());

  Replacement Replace2;
  EXPECT_TRUE(Replace2.getFilePath().empty());
}

TEST_F(ReplacementTest, CanApplyReplacements) {
  FileID ID = Context.createInMemoryFile("input.cpp",
                                         "line1\nline2\nline3\nline4");
  Replacements Replaces;
  Replaces.insert(Replacement(Context.Sources, Context.getLocation(ID, 2, 1),
                              5, "replaced"));
  Replaces.insert(Replacement(Context.Sources, Context.getLocation(ID, 3, 1),
                              5, "other"));
  EXPECT_TRUE(applyAllReplacements(Replaces, Context.Rewrite));
  EXPECT_EQ("line1\nreplaced\nother\nline4", Context.getRewrittenText(ID));
}

TEST_F(ReplacementTest, SkipsDuplicateReplacements) {
  FileID ID = Context.createInMemoryFile("input.cpp",
                                         "line1\nline2\nline3\nline4");
  Replacements Replaces;
  Replaces.insert(Replacement(Context.Sources, Context.getLocation(ID, 2, 1),
                              5, "replaced"));
  Replaces.insert(Replacement(Context.Sources, Context.getLocation(ID, 2, 1),
                              5, "replaced"));
  Replaces.insert(Replacement(Context.Sources, Context.getLocation(ID, 2, 1),
                              5, "replaced"));
  EXPECT_TRUE(applyAllReplacements(Replaces, Context.Rewrite));
  EXPECT_EQ("line1\nreplaced\nline3\nline4", Context.getRewrittenText(ID));
}

TEST_F(ReplacementTest, ApplyAllFailsIfOneApplyFails) {
  // This test depends on the value of the file name of an invalid source
  // location being in the range ]a, z[.
  FileID IDa = Context.createInMemoryFile("a.cpp", "text");
  FileID IDz = Context.createInMemoryFile("z.cpp", "text");
  Replacements Replaces;
  Replaces.insert(Replacement(Context.Sources, Context.getLocation(IDa, 1, 1),
                              4, "a"));
  Replaces.insert(Replacement(Context.Sources, SourceLocation(),
                              5, "2"));
  Replaces.insert(Replacement(Context.Sources, Context.getLocation(IDz, 1, 1),
                              4, "z"));
  EXPECT_FALSE(applyAllReplacements(Replaces, Context.Rewrite));
  EXPECT_EQ("a", Context.getRewrittenText(IDa));
  EXPECT_EQ("z", Context.getRewrittenText(IDz));
}

class FlushRewrittenFilesTest : public ::testing::Test {
 public:
  FlushRewrittenFilesTest() {
    std::string ErrorInfo;
    TemporaryDirectory = llvm::sys::Path::GetTemporaryDirectory(&ErrorInfo);
    assert(ErrorInfo.empty());
  }

  ~FlushRewrittenFilesTest() {
    std::string ErrorInfo;
    TemporaryDirectory.eraseFromDisk(true, &ErrorInfo);
    assert(ErrorInfo.empty());
  }

  FileID createFile(llvm::StringRef Name, llvm::StringRef Content) {
    llvm::SmallString<1024> Path(TemporaryDirectory.str());
    llvm::sys::path::append(Path, Name);
    std::string ErrorInfo;
    llvm::raw_fd_ostream OutStream(Path.c_str(),
                                   ErrorInfo, llvm::raw_fd_ostream::F_Binary);
    assert(ErrorInfo.empty());
    OutStream << Content;
    OutStream.close();
    const FileEntry *File = Context.Files.getFile(Path);
    assert(File != NULL);
    return Context.Sources.createFileID(File, SourceLocation(), SrcMgr::C_User);
  }

  std::string getFileContentFromDisk(llvm::StringRef Name) {
    llvm::SmallString<1024> Path(TemporaryDirectory.str());
    llvm::sys::path::append(Path, Name);
    // We need to read directly from the FileManager without relaying through
    // a FileEntry, as otherwise we'd read through an already opened file
    // descriptor, which might not see the changes made.
    // FIXME: Figure out whether there is a way to get the SourceManger to
    // reopen the file.
    return Context.Files.getBufferForFile(Path, NULL)->getBuffer();
  }

  llvm::sys::Path TemporaryDirectory;
  RewriterTestContext Context;
};

TEST_F(FlushRewrittenFilesTest, StoresChangesOnDisk) {
  FileID ID = createFile("input.cpp", "line1\nline2\nline3\nline4");
  Replacements Replaces;
  Replaces.insert(Replacement(Context.Sources, Context.getLocation(ID, 2, 1),
                              5, "replaced"));
  EXPECT_TRUE(applyAllReplacements(Replaces, Context.Rewrite));
  EXPECT_FALSE(Context.Rewrite.overwriteChangedFiles());
  EXPECT_EQ("line1\nreplaced\nline3\nline4",
            getFileContentFromDisk("input.cpp"));
}

namespace {
template <typename T>
class TestVisitor : public lfort::RecursiveASTVisitor<T> {
public:
  bool runOver(StringRef Code) {
    return runToolOnCode(new TestAction(this), Code);
  }

protected:
  lfort::SourceManager *SM;

private:
  class FindConsumer : public lfort::ASTConsumer {
  public:
    FindConsumer(TestVisitor *Visitor) : Visitor(Visitor) {}

    virtual void HandleProgram(lfort::ASTContext &Context) {
      Visitor->TraverseDecl(Context.getProgramDecl());
    }

  private:
    TestVisitor *Visitor;
  };

  class TestAction : public lfort::ASTFrontendAction {
  public:
    TestAction(TestVisitor *Visitor) : Visitor(Visitor) {}

    virtual lfort::ASTConsumer* CreateASTConsumer(
        lfort::CompilerInstance& compiler, llvm::StringRef dummy) {
      Visitor->SM = &compiler.getSourceManager();
      /// TestConsumer will be deleted by the framework calling us.
      return new FindConsumer(Visitor);
    }

  private:
    TestVisitor *Visitor;
  };
};
} // end namespace

// FIXME: Convert to Fortran
#if 0

void expectReplacementAt(const Replacement &Replace,
                         StringRef File, unsigned Offset, unsigned Length) {
  ASSERT_TRUE(Replace.isApplicable());
  EXPECT_EQ(File, Replace.getFilePath());
  EXPECT_EQ(Offset, Replace.getOffset());
  EXPECT_EQ(Length, Replace.getLength());
}

class ClassDeclXVisitor : public TestVisitor<ClassDeclXVisitor> {
public:
  bool VisitCXXRecordDecl(CXXRecordDecl *Record) {
    if (Record->getName() == "X") {
      Replace = Replacement(*SM, Record, "");
    }
    return true;
  }
  Replacement Replace;
};

TEST(Replacement, CanBeConstructedFromNode) {
  ClassDeclXVisitor ClassDeclX;
  EXPECT_TRUE(ClassDeclX.runOver("     class X;"));
  expectReplacementAt(ClassDeclX.Replace, "input.cc", 5, 7);
}

TEST(Replacement, ReplacesAtSpellingLocation) {
  ClassDeclXVisitor ClassDeclX;
  EXPECT_TRUE(ClassDeclX.runOver("#define A(Y) Y\nA(class X);"));
  expectReplacementAt(ClassDeclX.Replace, "input.cc", 17, 7);
}

class CallToFVisitor : public TestVisitor<CallToFVisitor> {
public:
  bool VisitCallExpr(CallExpr *Call) {
    if (Call->getDirectCallee()->getName() == "F") {
      Replace = Replacement(*SM, Call, "");
    }
    return true;
  }
  Replacement Replace;
};

TEST(Replacement, SubprogramCall) {
  CallToFVisitor CallToF;
  EXPECT_TRUE(CallToF.runOver("void F(); void G() { F(); }"));
  expectReplacementAt(CallToF.Replace, "input.cc", 21, 3);
}

TEST(Replacement, TemplatedSubprogramCall) {
  CallToFVisitor CallToF;
  EXPECT_TRUE(CallToF.runOver(
        "template <typename T> void F(); void G() { F<int>(); }"));
  expectReplacementAt(CallToF.Replace, "input.cc", 43, 8);
}

#endif

} // end namespace tooling
} // end namespace lfort
