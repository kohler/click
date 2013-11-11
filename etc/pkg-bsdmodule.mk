# pkg-bsdmodule.mk -- build tools for Click
# Jimmy Kjällman, 2011, Ericsson. Based on pkg-linuxmodule.mk.
#
# Eddie Kohler
#
# Copyright (c) 2006 Regents of the University of California
# Copyright (c) 2008 Meraki, Inc.
# Copyright (c) 2011 Jimmy Kjällman
# Copyright (c) 2011 Eddie Kohler
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
CFLAGS ?= $(CLICKKERNEL_CFLAGS)
CXXFLAGS ?= $(CLICKKERNEL_CXXFLAGS)
DEPCFLAGS ?= $(CLICKDEPCFLAGS)

DEFS ?= $(CLICKDEFS)
INCLUDES ?= -I$(clickbuild_includedir) -I$(clickbuild_srcdir)
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
PACKAGE_OBJS ?= bpackage.b.o
PACKAGE_DEPS ?=

CXXCOMPILE = $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PACKAGE_CXXFLAGS) $(DEFS) $(INCLUDES) $(DEPCFLAGS)
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@
COMPILE = $(CC) $(CPPFLAGS) $(CFLAGS) $(PACKAGE_CFLAGS) $(DEFS) $(INCLUDES) $(DEPCFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@
FIXDEP = @-sed 's/\.o:/.b.o:/' < $*.d > $*.b.d; /bin/rm -f $*.d

ifeq ($(V),1)
ccompile = $(COMPILE) $(1)
cxxcompile = $(CXXCOMPILE) $(1)
else
ccompile = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
cxxcompile = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
endif

.SUFFIXES:

%.b.o: %.c
	$(call ccompile,-c $< -o $@,CC)
	$(FIXDEP)
%.b.o: %.cc
	$(call cxxcompile,-c $< -o $@,CXX)
	$(FIXDEP)
%.b.ii: %.cc
	$(call cxxcompile,-E $< > $@,CXXCPP)

ifneq ($(MAKECMDGOALS),clean)
-include belements.mk
endif

OBJS = $(ELEMENT_OBJS) $(PACKAGE_OBJS)

$(package).bo: $(clickbuild_datadir)/pkg-bsdmodule.mk $(OBJS) $(PACKAGE_DEPS)
	$(LD) -Bshareable -o $(package).bo $(OBJS)
#	$(STRIP) -g $(package).bo

elemlist belements.conf: $(CLICK_BUILDTOOL)
	echo $(packagesrcdir) | $(CLICK_BUILDTOOL) $(CLICK_BUILDTOOL_FLAGS) findelem -r bsdmodule -r $(package) -P $(CLICKFINDELEMFLAGS) > belements.conf
belements.mk: belements.conf $(CLICK_BUILDTOOL)
	$(CLICK_BUILDTOOL) $(CLICK_BUILDTOOL_FLAGS) elem2make -t bsdmodule < belements.conf > belements.mk
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
	-rm -f *.b.d *.b.o belements.conf belements.mk bpackage.cc

.PHONY: clean elemlist always
