################################################################################
#
# 
# Copyright(c) 1999 - 2002 Intel Corporation. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation; either version 2 of the License, or (at your option) 
# any later version.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT 
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
# more details.
# 
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 
# Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# 
# The full GNU General Public License is included in this distribution in the
# file called LICENSE.
# 
# Contact Information:
# Linux NICS <linux.nics@intel.com>
# Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
#
################################################################################

###########################################################################
# Configuration Section

# Optional features - set to 'y' for on, anything else for off
# Intel(R) Advanced Network Services
IANS := n
# Intel(R) PRO Diagnostics
IDIAG := n

###########################################################################
# Driver files

# core driver files
TARGET = e1000.o
CFILES = e1000_main.c e1000_hw.c e1000_param.c \
         e1000_ethtool.c e1000_proc.c kcompat.c
HFILES = e1000.h e1000_hw.h e1000_osdep.h kcompat.h

# man page
MANSECTION = 7
MANFILE = $(TARGET:.o=.$(MANSECTION))

# iANS
IANS_CFLAGS = -DIANS -DIANS_BASE_ADAPTER_TEAMING \
              -DIANS_BASE_VLAN_TAGGING -DIANS_BASE_VLAN_ID
IANS_CFILES = ans.c ans_hw.c ans_os.c ans_driver.c
IANS_HFILES = base_comm.h ans_interface.h ans.h ans_hw.h ans_os.h ans_driver.h

# Diagnostics
IDIAG_CFLAGS = -DIDIAG
IDIAG_CFILES = e1000_idiag.c
IDIAG_HFILES = idiag_pro.h idiag_e1000.h

# Test Tools
IDIAG_TEST = idiag_test

###########################################################################
# Environment tests

# Kernel Search Path
# All the places we look for kernel source
KSP :=  /lib/modules/$(shell uname -r)/build \
        /usr/src/linux-$(shell uname -r) \
        /usr/src/linux-$(shell uname -r | sed 's/-.*//') \
        /usr/src/kernel-headers-$(shell uname -r) \
        /usr/src/kernel-source-$(shell uname -r) \
        /usr/src/linux-$(shell uname -r | sed 's/\([0-9]*\.[0-9]*\)\..*/\1/') \
        /usr/src/linux

# prune the list down to only values that exist
# and have an include/linux sub-directory
test_dir = $(shell [ -e $(dir)/include/linux ] && echo $(dir))
KSP := $(foreach dir, $(KSP), $(test_dir))

# we will use this first valid entry in the search path
ifeq (,$(KSRC))
  KSRC := $(firstword $(KSP))
endif

ifeq (,$(KSRC))
  $(error Linux kernel source not found)
endif

# files we get information from in KSRC
VERSION_FILE := $(KSRC)/include/linux/version.h
CONFIG_FILE  := $(KSRC)/include/linux/config.h

ifeq (,$(wildcard $(VERSION_FILE)))
  $(error Linux kernel source not configured - missing version.h)
endif

ifeq (,$(wildcard $(CONFIG_FILE)))
  $(error Linux kernel source not configured - missing config.h)
endif

# pick a compiler
ifneq (,$(findstring egcs-2.91.66, $(shell cat /proc/version)))
  CC := kgcc gcc cc
else
  CC := gcc cc
endif
test_cc = $(shell which $(cc) > /dev/null 2>&1 && echo $(cc))
CC := $(foreach cc, $(CC), $(test_cc))
CC := $(firstword $(CC))

# standard flags for module builds
CFLAGS += -DLINUX -D__KERNEL__ -DMODULE -O2 -pipe -Wall
CFLAGS += -I$(KSRC)/include -I.
CFLAGS += $(shell [ -f $(KSRC)/include/linux/modversions.h ] && \
            echo "-DMODVERSIONS -DEXPORT_SYMTAB \
                  -include $(KSRC)/include/linux/modversions.h")
CFLAGS += $(CFLAGS_EXTRA)

RHC := $(KSRC)/include/linux/rhconfig.h
ifneq (,$(wildcard $(RHC)))
	# 7.3 typo in rhconfig.h
	ifneq (,$(shell $(CC) $(CFLAGS) -E -dM $(RHC) | grep __module__bigmem))
		CFLAGS += -D__module_bigmem
	endif
endif

# get the kernel version - we use this to find the correct install path
KVER := $(shell $(CC) $(CFLAGS) -E -dM $(VERSION_FILE) | grep UTS_RELEASE | \
          awk '{ print $$3 }' | sed 's/\"//g')

ifneq ($(KVER),$(shell uname -r))
  $(warning ***)
  $(warning *** Warning: kernel source version ($(KVER)))
  $(warning *** does not match running kernel  ($(shell uname -r)))
  $(warning *** Continuing with build,)
  $(warning *** resulting driver may not be what you want)
  $(warning ***)
endif

# pick an appropriate install path
ifneq (,$(wildcard /lib/modules/$(KVER)/kernel))
  INSTDIR := /lib/modules/$(KVER)/kernel/drivers/net
else
  INSTDIR := /lib/modules/$(KVER)/net
endif

# look for SMP in config.h
SMP := $(shell $(CC) $(CFLAGS) -E -dM $(CONFIG_FILE) | \
         grep CONFIG_SMP | awk '{ print $$3 }')
ifneq ($(SMP),1)
  SMP := 0
endif

ifneq ($(SMP),$(shell uname -a | grep SMP > /dev/null 2>&1 && echo 1 || echo 0))
  $(warning ***)
ifeq ($(SMP),1)
  $(warning *** Warning: kernel source configuration (SMP))
  $(warning *** does not match running kernel (UP))
else
  $(warning *** Warning: kernel source configuration (UP))
  $(warning *** does not match running kernel (SMP))
endif
  $(warning *** Continuing with build,)
  $(warning *** resulting driver may not be what you want)
  $(warning ***)
endif

ifeq ($(SMP),1)
  CFLAGS += -D__SMP__
endif

ifeq (,$(MANDIR))
  # find the best place to install the man page
  MANPATH := $(shell (manpath 2>/dev/null || echo $MANPATH) | sed 's/:/ /g')
  ifneq (,$(MANPATH))
    # test based on inclusion in MANPATH
    test_dir = $(findstring $(dir), $(MANPATH))
  else
    # no MANPATH, test based on directory existence
    test_dir = $(shell [ -e $(dir) ] && echo $(dir))
  endif
  # our preferred install path
  # should /usr/local/man be in here ?
  MANDIR := /usr/share/man /usr/man
  MANDIR := $(foreach dir, $(MANDIR), $(test_dir))
  MANDIR := $(firstword $(MANDIR))
endif
ifeq (,$(MANDIR))
  # fallback to /usr/man
  MANDIR := /usr/man
endif

# we need to know what platform the driver is being built on
# some additional features are only built on Intel platforms
ARCH := $(shell uname -m | sed 's/i.86/i386/')
ifeq (,$(findstring $(ARCH),i386 ia64))
  IANS := n
endif

# depmod version for rpm builds
DEPVER := $(shell /sbin/depmod -V 2>/dev/null | awk 'BEGIN {FS="."} NR==1 {print $$2}') 

# iANS
ifeq ($(IANS),y)
  CFLAGS += $(IANS_CFLAGS)
  CFILES += $(IANS_CFILES)
  HFILES += $(IANS_HFILES)
endif

# Diagnostic features
ifeq ($(IDIAG),y)
  CFLAGS += $(IDIAG_CFLAGS)
  CFILES += $(IDIAG_CFILES)
  HFILES += $(IDIAG_HFILES)
endif

###########################################################################
# Build rules

.SILENT: $(TARGET)
$(TARGET): $(filter-out $(TARGET), $(CFILES:.c=.o))
	$(LD) -r $^ -o $@
	echo; echo
	echo "**************************************************"
	echo "** $(TARGET) built for $(KVER)"
	echo -n "** SMP               "
	if [ "$(SMP)" = "1" ]; \
		then echo "Enabled"; else echo "Disabled"; fi
	echo -n "** iANS hooks        "
	if [ "$(IANS)" = "y" ]; \
		then echo "Enabled"; else echo "Disabled"; fi
	echo -n "** Pro Diagnostics   "
	if [ "$(IDIAG)" = "y" ]; \
		then echo "Enabled"; else echo "Disabled"; fi
	echo "**************************************************"
	echo

$(CFILES:.c=.o): $(HFILES) Makefile

$(MANFILE).gz: ../$(MANFILE)
	gzip -c $< > $@

install: $(TARGET) $(MANFILE).gz
	# remove all old versions of the driver
	find $(INSTALL_MOD_PATH)/lib/modules/$(KVER) -name $(TARGET) -exec rm {} \; \
		|| true
	install -D -m 644 $(TARGET) $(INSTALL_MOD_PATH)$(INSTDIR)/$(TARGET)
ifeq (,$(INSTALL_MOD_PATH))
	/sbin/depmod -a || true
else
  ifeq ($(DEPVER),1 )
	/sbin/depmod -r $(INSTALL_MOD_PATH) -a || true
  else
	/sbin/depmod -b $(INSTALL_MOD_PATH) -a || true
  endif
endif
	install -D -m 644 $(MANFILE).gz $(INSTALL_MOD_PATH)$(MANDIR)/man$(MANSECTION)/$(MANFILE).gz
	man -c -P'cat > /dev/null' $(MANFILE:.$(MANSECTION)=) || true

uninstall:
	if [ -e $(INSTDIR)/$(TARGET) ] ; then \
	    rm -f $(INSTDIR)/$(TARGET) ; \
	fi
	/sbin/depmod -a
	if [ -e $(MANDIR)/man$(MANSECTION)/$(MANFILE).gz ] ; then \
		rm -f $(MANDIR)/man$(MANSECTION)/$(MANFILE).gz ; \
	fi

$(IDIAG_TEST): $(IDIAG_TEST).c Makefile
	$(CC) -g -I. $(IDIAG_TEST).c -o $@

clean:
	rm -f $(TARGET) $(CFILES:.c=.o) $(MANFILE).gz $(IDIAG_TEST)

