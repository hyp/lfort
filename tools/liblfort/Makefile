##===- tools/liblfort/Makefile -----------------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LFORT_LEVEL := ../..
LIBRARYNAME = lfort

EXPORTED_SYMBOL_FILE = $(PROJ_SRC_DIR)/liblfort.exports

LINK_LIBS_IN_SHARED = 1
SHARED_LIBRARY = 1

include $(LFORT_LEVEL)/../../Makefile.config
LINK_COMPONENTS := $(TARGETS_TO_BUILD) asmparser support mc
USEDLIBS = lfortRewriteCore.a lfortRewriteFrontend.a \
	   lfortFrontend.a lfortDriver.a \
	   lfortSerialization.a \
	   lfortParse.a lfortSema.a lfortEdit.a lfortAnalysis.a \
	   lfortAST.a lfortLex.a lfortTooling.a lfortBasic.a \
	   lfortFormat.a 

include $(LFORT_LEVEL)/Makefile

# Add soname to the library.
ifeq ($(HOST_OS), $(filter $(HOST_OS), Linux FreeBSD GNU))
        LDFLAGS += -Wl,-soname,lib$(LIBRARYNAME)$(SHLIBEXT)
endif

##===----------------------------------------------------------------------===##
# FIXME: This is copied from the 'lto' makefile.  Should we share this?
##===----------------------------------------------------------------------===##

ifeq ($(HOST_OS),Darwin)
    LLVMLibsOptions += -Wl,-compatibility_version,1

    # Set dylib internal version number to submission number.
    ifdef LLVM_SUBMIT_VERSION
        LLVMLibsOptions += -Wl,-current_version \
                           -Wl,$(LLVM_SUBMIT_VERSION).$(LLVM_SUBMIT_SUBVERSION)
    endif

    # Extra options to override libtool defaults.
    LLVMLibsOptions += -Wl,-dead_strip

    # Mac OS X 10.4 and earlier tools do not allow a second -install_name on command line
    DARWIN_VERS := $(shell echo $(TARGET_TRIPLE) | sed 's/.*darwin\([0-9]*\).*/\1/')
    ifneq ($(DARWIN_VERS),8)
       LLVMLibsOptions += -Wl,-install_name \
                          -Wl,"@rpath/lib$(LIBRARYNAME)$(SHLIBEXT)"
    endif

    # If we're doing an Apple-style build, add the LTO object path.
    ifeq ($(RC_BUILDIT),YES)
       TempFile        := $(shell mkdir -p ${OBJROOT}/dSYMs ; mktemp ${OBJROOT}/dSYMs/lfort-lto.XXXXXX)
       LLVMLibsOptions += -Wl,-object_path_lto -Wl,$(TempFile)
    endif
endif
