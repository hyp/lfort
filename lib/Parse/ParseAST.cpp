//===--- ParseAST.cpp - Provide the lfort::ParseAST method ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the lfort::ParseAST method.
//
//===----------------------------------------------------------------------===//

#include "lfort/Parse/ParseAST.h"
#include "lfort/AST/ASTConsumer.h"
#include "lfort/AST/ASTContext.h"
#include "lfort/AST/DeclCXX.h"
#include "lfort/AST/ExternalASTSource.h"
#include "lfort/AST/Stmt.h"
#include "lfort/Parse/ParseDiagnostic.h"
#include "lfort/Parse/Parser.h"
#include "lfort/Sema/CodeCompleteConsumer.h"
#include "lfort/Sema/ExternalSemaSource.h"
#include "lfort/Sema/Sema.h"
#include "lfort/Sema/SemaConsumer.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/CrashRecoveryContext.h"
#include <cstdio>

using namespace lfort;

namespace {

/// If a crash happens while the parser is active, an entry is printed for it.
class PrettyStackTraceParserEntry : public llvm::PrettyStackTraceEntry {
  const Parser &P;
public:
  PrettyStackTraceParserEntry(const Parser &p) : P(p) {}
  virtual void print(raw_ostream &OS) const;
};

/// If a crash happens while the parser is active, print out a line indicating
/// what the current token is.
void PrettyStackTraceParserEntry::print(raw_ostream &OS) const {
  const Token &Tok = P.getCurToken();
  if (Tok.is(tok::eof)) {
    OS << "<eof> parser at end of file\n";
    return;
  }

  if (Tok.getLocation().isInvalid()) {
    OS << "<unknown> parser at unknown location\n";
    return;
  }

  const Preprocessor &PP = P.getPreprocessor();
  Tok.getLocation().print(OS, PP.getSourceManager());
  if (Tok.isAnnotation())
    OS << ": at annotation token \n";
  else
    OS << ": current parser token '" << PP.getSpelling(Tok) << "'\n";
}

}  // namespace

//===----------------------------------------------------------------------===//
// Public interface to the file
//===----------------------------------------------------------------------===//

/// ParseAST - Parse the entire file specified, notifying the ASTConsumer as
/// the file is parsed.  This inserts the parsed decls into the translation unit
/// held by Ctx.
///
void lfort::ParseAST(Preprocessor &PP, ASTConsumer *Consumer,
                     ASTContext &Ctx, bool PrintStats,
                     TranslationUnitKind TUKind,
                     CodeCompleteConsumer *CompletionConsumer,
                     bool SkipSubprogramBodies) {

  OwningPtr<Sema> S(new Sema(PP, Ctx, *Consumer, TUKind, CompletionConsumer));

  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<Sema> CleanupSema(S.get());
  
  ParseAST(*S.get(), PrintStats, SkipSubprogramBodies);
}

void lfort::ParseAST(Sema &S, bool PrintStats, bool SkipSubprogramBodies) {
  // Collect global stats on Decls/Stmts (until we have a module streamer).
  if (PrintStats) {
    Decl::EnableStatistics();
    Stmt::EnableStatistics();
  }

  // Also turn on collection of stats inside of the Sema object.
  bool OldCollectStats = PrintStats;
  std::swap(OldCollectStats, S.CollectStats);

  ASTConsumer *Consumer = &S.getASTConsumer();

  OwningPtr<Parser> ParseOP(new Parser(S.getPreprocessor(), S,
                                       SkipSubprogramBodies));
  Parser &P = *ParseOP.get();

  PrettyStackTraceParserEntry CrashInfo(P);

  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<Parser>
    CleanupParser(ParseOP.get());

  S.getPreprocessor().EnterMainSourceFile();
  P.Initialize();

  // Fortran allows empty inputs as a valid program.
  Parser::DeclGroupPtrTy ADecl;
  ExternalASTSource *External = S.getASTContext().getExternalSource();
  if (External)
    External->StartTranslationUnit(Consumer);

  if (!P.ParseTopLevelDecl(ADecl)) {
    do {
      // If we got a null return and something *was* parsed, ignore it.  This
      // is due to a top-level semicolon, an action override, or a parse error
      // skipping something.
      if (ADecl && !Consumer->HandleTopLevelDecl(ADecl.get()))
        return;
    } while (!P.ParseTopLevelDecl(ADecl));
  }

  // Process any TopLevelDecls generated by #pragma weak.
  for (SmallVector<Decl*,2>::iterator
       I = S.WeakTopLevelDecls().begin(),
       E = S.WeakTopLevelDecls().end(); I != E; ++I)
    Consumer->HandleTopLevelDecl(DeclGroupRef(*I));
  
  Consumer->HandleTranslationUnit(S.getASTContext());

  std::swap(OldCollectStats, S.CollectStats);
  if (PrintStats) {
    llvm::errs() << "\nSTATISTICS:\n";
    P.getActions().PrintStats();
    S.getASTContext().PrintStats();
    Decl::PrintStats();
    Stmt::PrintStats();
    Consumer->PrintStats();
  }
}
