#
# Makefile for Intel(R) PRO/1000 LAN Adapter driver for Linux
# 			Copyright (C) 1999 - 2000 Intel
#

CC = gcc

LINUX = /usr/src/linux

# Required Flags
CFLAGS = -DLINUX -D__KERNEL__ -DHAVE_INTEL_CPU -DMODULE -DEXPORT_SYMTAB -O2 -pipe -I. \
-I$(LINUX)/include

# Check for SMP
CFLAGS += $(subst SMP, -D__SMP__, $(findstring SMP, $(shell uname -v)))

# Check for Module Versioning
CFLAGS += $(shell [ -f /usr/include/linux/modversions.h ] && echo -DMODVERSIONS)

# Check for a Click polling kernel
CFLAGS += $(shell grep -q polling $(LINUX)/include/linux/netdevice.h && echo -DCLICK_POLLING)

TARGET = e1000.o
CFILES = e1000_main.c e1000_fxhw.c e1000_phy.c e1000_proc.c

# ANS, ia32 2.2.x only
ARCH = $(shell uname -m | sed -e s/i.86/i386/)
KVER = $(shell uname -r | sed -e s/2\.2.*/2.2/)
ifeq ($(ARCH), i386)
ifeq ($(KVER), 2.2)
# CFILES += ans.c ans_hw.c ans_os.c ans_driver.c
# CFLAGS += -DIANS -DIANS_BASE_ADAPTER_TEAMING
# IANS = y
endif
endif

INSTDIR = /lib/modules/$(shell uname -r)/net

.SILENT: all
all: $(TARGET)
	echo; echo; \
	echo "**************************************************"; \
	echo "** $(TARGET) built for for $(shell uname -s -r)"; \
	if uname -v | grep SMP > /dev/null; then \
		echo "** SMP Enabled";               \
	else                                     \
		echo "** SMP Disabled";              \
	fi;                                      \
	if grep -q polling $(LINUX)/include/linux/netdevice.h ; then \
		echo '** Click Polling Enabled'; \
	else \
		echo '** Click Polling Disabled'; \
        fi; \
	if [ -f /usr/include/linux/modversions.h ]; then \
		echo "** Module Versioning Enabled";         \
	else                                             \
		echo "** Module Versioning Disabled";        \
	fi;                                              \
	if echo $(IANS) | grep y > /dev/null; then \
		echo "** iANS hooks enabled"; \
	fi;
	echo "**************************************************"; 

$(TARGET): $(CFILES:.c=.o)
	$(LD) -r $^ -o $@

install: $(TARGET)
	mkdir -p $(INSTDIR)
	install -m 644 $(TARGET) $(INSTDIR)
	depmod -a

uninstall:
	if [ -f $(INSTDIR)/$(TARGET) ]; then \
		rm $(INSTDIR)/$(TARGET);         \
	fi

clean:
	rm -f *.o *~ core

