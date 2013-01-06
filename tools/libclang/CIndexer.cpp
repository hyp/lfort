//===- CIndex.cpp - LFort-C Source Indexing Library -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the LFort-C Source Indexing library.
//
//===----------------------------------------------------------------------===//

#include "CIndexer.h"
#include "lfort/AST/Decl.h"
#include "lfort/AST/DeclVisitor.h"
#include "lfort/AST/StmtVisitor.h"
#include "lfort/Basic/FileManager.h"
#include "lfort/Basic/SourceManager.h"
#include "lfort/Basic/Version.h"
#include "lfort/Sema/CodeCompleteConsumer.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
#include <sstream>
#include <vector>

#ifdef __CYGWIN__
#include <cygwin/version.h>
#include <sys/cygwin.h>
#define LLVM_ON_WIN32 1
#endif

#ifdef LLVM_ON_WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace lfort;

std::string CIndexer::getLFortResourcesPath() {
  // Did we already compute the path?
  if (!ResourcesPath.empty())
    return ResourcesPath.str();
  
  // Find the location where this library lives (liblfort.dylib).
#ifdef LLVM_ON_WIN32
  MEMORY_BASIC_INFORMATION mbi;
  char path[MAX_PATH];
  VirtualQuery((void *)(uintptr_t)lfort_createTranslationUnit, &mbi,
               sizeof(mbi));
  GetModuleFileNameA((HINSTANCE)mbi.AllocationBase, path, MAX_PATH);

#ifdef __CYGWIN__
  char w32path[MAX_PATH];
  strcpy(w32path, path);
#if CYGWIN_VERSION_API_MAJOR > 0 || CYGWIN_VERSION_API_MINOR >= 181
  cygwin_conv_path(CCP_WIN_A_TO_POSIX, w32path, path, MAX_PATH);
#else
  cygwin_conv_to_full_posix_path(w32path, path);
#endif
#endif

  llvm::sys::Path LibLFortPath(path);
  LibLFortPath.eraseComponent();
#else
  // This silly cast below avoids a C++ warning.
  Dl_info info;
  if (dladdr((void *)(uintptr_t)lfort_createTranslationUnit, &info) == 0)
    llvm_unreachable("Call to dladdr() failed");
  
  llvm::sys::Path LibLFortPath(info.dli_fname);
  
  // We now have the CIndex directory, locate lfort relative to it.
  LibLFortPath.eraseComponent();
#endif
  
  LibLFortPath.appendComponent("lfort");
  LibLFortPath.appendComponent(LFORT_VERSION_STRING);

  // Cache our result.
  ResourcesPath = LibLFortPath;
  return LibLFortPath.str();
}

static llvm::sys::Path GetTemporaryPath() {
  // FIXME: This is lame; sys::Path should provide this function (in particular,
  // it should know how to find the temporary files dir).
  std::string Error;
  const char *TmpDir = ::getenv("TMPDIR");
  if (!TmpDir)
    TmpDir = ::getenv("TEMP");
  if (!TmpDir)
    TmpDir = ::getenv("TMP");
  if (!TmpDir)
    TmpDir = "/tmp";
  llvm::sys::Path P(TmpDir);
  P.appendComponent("remap");
  if (P.makeUnique(false, &Error))
    return llvm::sys::Path("");

  // FIXME: Grumble, makeUnique sometimes leaves the file around!?  PR3837.
  P.eraseFromDisk(false, 0);

  return P;
}

bool lfort::RemapFiles(unsigned num_unsaved_files,
                       struct CXUnsavedFile *unsaved_files,
                       std::vector<std::string> &RemapArgs,
                       std::vector<llvm::sys::Path> &TemporaryFiles) {
  for (unsigned i = 0; i != num_unsaved_files; ++i) {
    // Write the contents of this unsaved file into the temporary file.
    llvm::sys::Path SavedFile(GetTemporaryPath());
    if (SavedFile.empty())
      return true;

    std::string ErrorInfo;
    llvm::raw_fd_ostream OS(SavedFile.c_str(), ErrorInfo,
                            llvm::raw_fd_ostream::F_Binary);
    if (!ErrorInfo.empty())
      return true;
    
    OS.write(unsaved_files[i].Contents, unsaved_files[i].Length);
    OS.close();
    if (OS.has_error()) {
      SavedFile.eraseFromDisk();
      OS.clear_error();
      return true;
    }
    
    // Remap the file.
    std::string RemapArg = unsaved_files[i].Filename;
    RemapArg += ';';
    RemapArg += SavedFile.str();
    RemapArgs.push_back("-Xlfort");
    RemapArgs.push_back("-remap-file");
    RemapArgs.push_back("-Xlfort");
    RemapArgs.push_back(RemapArg);
    TemporaryFiles.push_back(SavedFile);
  }
  
  return false;
}

