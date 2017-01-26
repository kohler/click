# pkg-linuxmodule.mk -- build tools for Click
# Eddie Kohler
#
# Copyright (c) 2006-2013 Eddie Kohler
# Copyright (c) 2006 Regents of the University of California
# Copyright (c) 2008 Meraki, Inc.
# Copyright (c) 2013 President and Fellows of Harvard College
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
PACKAGE_OBJS ?= $(package)-kmain.ko
PACKAGE_DEPS ?=

PACKAGE_CLEANFILES ?= $(package)-kelem.mk $(package)-kmain.cc kversion.c Kbuild
ifndef MINDRIVER
PACKAGE_CLEANFILES += $(package)-kelem.conf
endif

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

ifdef MINDRIVER
elemlist:
	@:
else
elemlist $(package)-kelem.conf: $(CLICK_BUILDTOOL)
	echo $(packagesrcdir) | $(CLICK_BUILDTOOL) $(CLICK_BUILDTOOL_FLAGS) findelem -r linuxmodule -r $(package) -P $(CLICKFINDELEMFLAGS) > $(package)-kelem.conf
endif
$(package)-kelem.mk: $(package)-kelem.conf $(CLICK_BUILDTOOL)
	$(CLICK_BUILDTOOL) $(CLICK_BUILDTOOL_FLAGS) elem2make --linux -t linuxmodule < $(package)-kelem.conf > $(package)-kelem.mk
$(package)-kmain.cc: $(package)-kelem.conf $(CLICK_BUILDTOOL)
	$(CLICK_ELEM2PACKAGE) $(package) < $(package)-kelem.conf > $(package)-kmain.cc
	@rm -f $(package)-kmain.kd
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
	-rm -f *.k.d *.k.o $(PACKAGE_CLEANFILES)
	-rm -f .*.o.cmd .*.ko.cmd $(package).mod.c $(package).mod.o $(package).o
	-rm -rf .tmp_versions

.PHONY: clean elemlist always
