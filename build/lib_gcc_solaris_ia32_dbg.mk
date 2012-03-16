#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information
#

#
# This makefile is for building the RSTM libraries and benchmarks using
# library API, GCC, Solaris, ia32, -O0
#
# NB: corei7 may not be available on older versions of gcc.  This makefile
#     assumes a 4.7-ish gcc.  Please adjust accordingly.
#
# Warning: This won't work without also including Rules.mk and Targets.mk,
#          but to avoid weird path issues, we include it from the invocation,
#          not from this file.
#

ODIR        = obj.lib_gcc_solaris_ia32_dbg
CONFIGH     = $(ODIR)/config.h
CXX         = g++
CXXFLAGS    = -O0 -ggdb -m32 -march=corei7 -mtune=corei7 -msse2 -mfpmath=sse
CXXFLAGS   += -DSINGLE_SOURCE_BUILD -I./$(ODIR) -I./include -I./common
LDFLAGS    += -lrt -lpthread -m32 -lmtmalloc

$(CONFIGH):
	@echo "// This file was auto-generated on " `date` > $@
	@echo "" >> $@
	@echo "#define STM_API_LIB" >> $@
	@echo "#define STM_CC_GCC" >> $@
	@echo "#define STM_OS_SOLARIS" >> $@
	@echo "#define STM_CPU_X86" >> $@
	@echo "#define STM_BITS_32" >> $@
	@echo "#define STM_OPT_O0" >> $@
	@echo "#define STM_WS_WORDLOG" >> $@
