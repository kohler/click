# pkg-linuxmodule.mk -- build tools for Click
# Eddie Kohler
#
# Copyright (c) 2006 Regents of the University of California
# Copyright (c) 2008 Meraki, Inc.
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

CLICKBUILD = linuxmodule

CC ?= $(CLICKKERNEL_CC)
CPP ?= $(CLICKCPP)
CXX ?= $(CLICKKERNEL_CXX)
CXXCPP ?= $(CLICKCXXCPP)
AR_CREATE ?= $(CLICKAR_CREATE)
RANLIB ?= $(CLICKRANLIB)
STRIP ?= $(CLICKSTRIP)

CPPFLAGS ?= $(CLICKCPPFLAGS) -DCLICK_LINUXMODULE
CFLAGS ?= $(CLICKKERNEL_CFLAGS)
CXXFLAGS ?= $(CLICKKERNEL_CXXFLAGS)
DEPCFLAGS ?= $(CLICKDEPCFLAGS)

DEFS ?= $(CLICKDEFS)
INCLUDES ?= -I$(clickbuild_includedir) -I$(clickbuild_srcdir)
LDFLAGS ?= $(CLICKLDFLAGS)

LINUX_MAKEARGS ?= $(CLICKLINUX_MAKEARGS)

packagesrcdir ?= $(srcdir)
PACKAGE_OBJS ?= kpackage.ko
PACKAGE_DEPS ?=

ifneq ($(CLICK_LINUXMODULE_2_6),1)

CXXCOMPILE = $(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) $(PACKAGE_CXXFLAGS) $(DEFS) $(INCLUDES)
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@
COMPILE = $(CC) $(CPPFLAGS) $(CFLAGS) $(PACKAGE_CFLAGS) $(DEFS) $(INCLUDES)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@
FIXDEP = @-sed 's/\.o:/\.ko:/' < $*.d > $*.kd; /bin/rm -f $*.d

ifeq ($(V),1)
ccompile = $(COMPILE) $(DEPCFLAGS) $(1)
ccompile_nodep = $(COMPILE) $(1)
cxxcompile = $(CXXCOMPILE) $(DEPCFLAGS) $(1)
cxxcompile_nodep = $(CXXCOMPILE) $(1)
else
ccompile = @/bin/echo ' ' $(2) $< && $(COMPILE) $(DEPCFLAGS) $(1)
ccompile_nodep = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
cxxcompile = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(DEPCFLAGS) $(1)
cxxcompile_nodep = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
endif

.SUFFIXES:
.SUFFIXES: .c .cc .ko .kii

.c.ko:
	$(call ccompile,-c $< -o $@,CC)
	$(FIXDEP)
.cc.ko:
	$(call cxxcompile,-c $< -o $@,CXX)
	$(FIXDEP)
.cc.kii:
	$(call cxxcompile_nodep,-E $< > $@,CXXCPP)

ifneq ($(MAKECMDGOALS),clean)
-include kelements.mk
endif

OBJS = $(ELEMENT_OBJS) $(PACKAGE_OBJS) kversion.ko

endif

ifeq ($(CLICK_LINUXMODULE_2_6),1)
# Jump through hoops to avoid missing symbol warnings
$(package).ko: Makefile Kbuild always $(PACKAGE_DEPS)
	@fifo=.$(package).ko.$$$$.$$RANDOM.errors; rm -f $$fifo; \
	command="$(MAKE) -C $(clicklinux_builddir) M=$(shell pwd) $(LINUX_MAKEARGS) CLICK_PACKAGE_MAKING=linuxmodule-26 modules"; \
	echo $$command; if mkfifo $$fifo; then \
	    (grep -iv '^[\* ]*Warning:.*undefined' <$$fifo 1>&2; rm -f $$fifo) & \
	    eval $$command 2>$$fifo; \
	else eval $$command; fi
Kbuild: $(CLICK_BUILDTOOL)
	echo 'include $$(obj)/Makefile' > Kbuild
	$(CLICK_BUILDTOOL) $(CLICK_BUILDTOOL_FLAGS) kbuild >> Kbuild
else
$(package).ko: $(clickbuild_datadir)/pkg-linuxmodule.mk $(OBJS) $(PACKAGE_DEPS)
	$(LD) -r -o $(package).ko $(OBJS)
	$(STRIP) -g $(package).ko
endif

elemlist kelements.conf: $(CLICK_BUILDTOOL)
	echo $(packagesrcdir) | $(CLICK_BUILDTOOL) $(CLICK_BUILDTOOL_FLAGS) findelem -r linuxmodule -r $(package) -P $(CLICKFINDELEMFLAGS) > kelements.conf
kelements.mk: kelements.conf $(CLICK_BUILDTOOL)
	$(CLICK_BUILDTOOL) $(CLICK_BUILDTOOL_FLAGS) elem2make -t linuxmodule < kelements.conf > kelements.mk
kpackage.cc: kelements.conf $(CLICK_BUILDTOOL)
	$(CLICK_ELEM2PACKAGE) $(package) < kelements.conf > kpackage.cc
	@rm -f kpackage.kd
kversion.c: $(CLICK_BUILDTOOL)
	$(CLICK_BUILDTOOL) $(CLICK_BUILDTOOL_FLAGS) kversion $(KVERSIONFLAGS) > kversion.c

DEPFILES := $(wildcard *.kd)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

always:
	@:
clean:
	-rm -f $(package).ko .$(package).ko.status
	-rm -f *.kd *.ko kelements.conf kelements.mk kpackage.cc kversion.c Kbuild
	-rm -f .*.o.cmd .*.ko.cmd $(package).mod.c $(package).mod.o $(package).o
	-rm -rf .tmp_versions

.PHONY: clean elemlist always
