# pkg-bsdmodule.mk -- build tools for Click
# Jimmy Kjällman, 2011, Ericsson. Based on pkg-linuxmodule.mk. 
#
# Eddie Kohler
#
# Copyright (c) 2006 Regents of the University of California
# Copyright (c) 2008 Meraki, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, subject to the conditions
# listed in the Click LICENSE file. These conditions include: you must
# preserve this copyright notice, and you cannot mention the copyright
# holders in advertising related to the Software without their permission.
# The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
# notice is a summary of the Click LICENSE file; the license in that file is
# legally binding.

CLICKBUILD = bsdmodule

CC ?= $(CLICKKERNEL_CC)
CPP ?= $(CLICKCPP)
CXX ?= $(CLICKKERNEL_CXX)
CXXCPP ?= $(CLICKCXXCPP)
LD ?= ld
AR_CREATE ?= $(CLICKAR_CREATE)  # ?
RANLIB ?= $(CLICKRANLIB)  # ?
STRIP ?= $(CLICKSTRIP)

CPPFLAGS ?= $(CLICKCPPFLAGS) -DCLICK_BSDMODULE
CFLAGS ?= $(CLICKCFLAGS_NDEBUG)
CXXFLAGS ?= $(CLICKCXXFLAGS_NDEBUG)
DEPCFLAGS ?= $(CLICKDEPCFLAGS)

DEFS ?= $(CLICKDEFS)
INCLUDES ?= -I$(clickincludedir) -I$(clicksrcdir)
LDFLAGS ?= $(CLICKLDFLAGS)

target_cpu ?= $(shell /usr/bin/uname -p)
ifneq ($(target_cpu),i386)
CFLAGS +=  -fPIC -fno-builtin
CXXFLAGS +=  -fPIC -fno-builtin
else
CFLAGS +=  -fno-builtin
CXXFLAGS +=  -fno-builtin
endif

DEFS +=  -D_KERNEL  # XXX

packagesrcdir ?= $(srcdir)
PACKAGE_OBJS ?= bpackage.bo
PACKAGE_DEPS ?=

CLICK_BUILDTOOL ?= $(clickbindir)/click-buildtool
CLICK_ELEM2PACKAGE ?= $(CLICK_BUILDTOOL) elem2package $(ELEM2PACKAGE_INCLUDES)

CXXCOMPILE = $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PACKAGE_CXXFLAGS) $(DEFS) $(INCLUDES) $(DEPCFLAGS)
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@
COMPILE = $(CC) $(CPPFLAGS) $(CFLAGS) $(PACKAGE_CFLAGS) $(DEFS) $(INCLUDES) $(DEPCFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@
FIXDEP = @-sed 's/\.o:/\.bo:/' < $*.d > $*.bd; /bin/rm -f $*.d

ifeq ($(V),1)
ccompile = $(COMPILE) $(1)
cxxcompile = $(CXXCOMPILE) $(1)
else
ccompile = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
cxxcompile = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
endif

.SUFFIXES:
.SUFFIXES: .c .cc .bo .bii

.c.bo:
	$(call ccompile,-c $< -o $@,CC)
	$(FIXDEP)
.cc.bo:
	$(call cxxcompile,-c $< -o $@,CXX)
	$(FIXDEP)
.cc.bii:
	$(call cxxcompile,-E $< > $@,CXXCPP)

ifneq ($(MAKECMDGOALS),clean)
-include belements.mk
endif

OBJS = $(ELEMENT_OBJS) $(PACKAGE_OBJS)

$(package).bo: $(clickdatadir)/pkg-bsdmodule.mk $(OBJS) $(PACKAGE_DEPS)
	$(LD) -Bshareable -o $(package).bo $(OBJS)
#	$(STRIP) -g $(package).bo

elemlist belements.conf: $(CLICK_BUILDTOOL)
	echo $(packagesrcdir) | $(CLICK_BUILDTOOL) findelem -r bsdmodule -r $(package) -P $(CLICKFINDELEMFLAGS) > belements.conf
belements.mk: belements.conf $(CLICK_BUILDTOOL)
	$(CLICK_BUILDTOOL) elem2make -t bsdmodule < belements.conf > belements.mk
bpackage.cc: belements.conf $(CLICK_BUILDTOOL)
	$(CLICK_ELEM2PACKAGE) $(package) < belements.conf > bpackage.cc
	@rm -f bpackage.bd

DEPFILES := $(wildcard *.bd)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

always:
	@:
clean:
	-rm -f $(package).bo
	-rm -f *.bd *.bo belements.conf belements.mk bpackage.cc

.PHONY: clean elemlist always
