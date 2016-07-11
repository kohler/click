ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif
ifeq ($(RTE_TARGET),)
$(error "Please define RTE_TARGET environment variable")
endif

# Save C flags
DPDK_OLD_CFLAGS := $(CFLAGS)

include $(RTE_SDK)/mk/rte.vars.mk

LIBS += -L$(RTE_SDK_BIN)/lib

# Include libraries depending on config if NO_AUTOLIBS is not set
# Order is important: from higher level to lower level

ifeq ($(NO_AUTOLIBS),)

LIBS += -Wl,--whole-archive

ifeq ($(CONFIG_RTE_LIBRTE_DISTRIBUTOR),y)
LIBS += -lrte_distributor
endif

ifeq ($(CONFIG_RTE_LIBRTE_KNI),y)
ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP),y)
LIBS += -lrte_kni
endif
endif

ifeq ($(CONFIG_RTE_LIBRTE_IVSHMEM),y)
ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP),y)
LIBS += -lrte_ivshmem
endif
endif

ifeq ($(CONFIG_RTE_LIBRTE_PIPELINE),y)
LIBS += -lrte_pipeline
endif

ifeq ($(CONFIG_RTE_LIBRTE_TABLE),y)
LIBS += -lrte_table
endif

ifeq ($(CONFIG_RTE_LIBRTE_PORT),y)
LIBS += -lrte_port
endif

ifeq ($(CONFIG_RTE_LIBRTE_TIMER),y)
LIBS += -lrte_timer
endif

ifeq ($(CONFIG_RTE_LIBRTE_HASH),y)
LIBS += -lrte_hash
endif

ifeq ($(CONFIG_RTE_LIBRTE_LPM),y)
LIBS += -lrte_lpm
endif

ifeq ($(CONFIG_RTE_LIBRTE_POWER),y)
LIBS += -lrte_power
endif

ifeq ($(CONFIG_RTE_LIBRTE_ACL),y)
LIBS += -lrte_acl
endif

ifeq ($(CONFIG_RTE_LIBRTE_METER),y)
LIBS += -lrte_meter
endif

ifeq ($(CONFIG_RTE_LIBRTE_SCHED),y)
LIBS += -lrte_sched
LIBS += -lm
LIBS += -lrt
endif

LIBS += -Wl,--start-group

ifeq ($(CONFIG_RTE_LIBRTE_KVARGS),y)
LIBS += -lrte_kvargs
endif

ifeq ($(CONFIG_RTE_LIBRTE_MBUF),y)
LIBS += -lrte_mbuf
endif

ifeq ($(CONFIG_RTE_LIBRTE_IP_FRAG),y)
LIBS += -lrte_ip_frag
endif

ifeq ($(CONFIG_RTE_LIBRTE_ETHER),y)
LIBS += -lethdev
endif

ifeq ($(CONFIG_RTE_LIBRTE_MALLOC),y)
LIBS += -lrte_malloc
endif

ifeq ($(CONFIG_RTE_LIBRTE_MEMPOOL),y)
LIBS += -lrte_mempool
endif

ifeq ($(CONFIG_RTE_LIBRTE_RING),y)
LIBS += -lrte_ring
endif

ifeq ($(CONFIG_RTE_LIBC),y)
LIBS += -lc
LIBS += -lm
endif

ifeq ($(CONFIG_RTE_LIBGLOSS),y)
LIBS += -lgloss
endif

ifeq ($(CONFIG_RTE_LIBRTE_EAL),y)
LIBS += -lrte_eal
endif

ifeq ($(CONFIG_RTE_LIBRTE_CMDLINE),y)
LIBS += -lrte_cmdline
endif

ifeq ($(CONFIG_RTE_LIBRTE_CFGFILE),y)
LIBS += -lrte_cfgfile
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_BOND),y)
LIBS += -lrte_pmd_bond
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_XENVIRT),y)
LIBS += -lrte_pmd_xenvirt
LIBS += -lxenstore
endif

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),n)
# Plugins (link only if static libraries)

ifeq ($(CONFIG_RTE_LIBRTE_VMXNET3_PMD),y)
LIBS += -lrte_pmd_vmxnet3_uio
endif

ifeq ($(CONFIG_RTE_LIBRTE_VIRTIO_PMD),y)
ifeq ($(shell [ \( $(RTE_VER_MAJOR) -ge 2 \) -a \( $(RTE_VER_MINOR) -ge 1  \) ] && echo true),true)
  LIBS += -lrte_pmd_virtio
else
  LIBS += -lrte_pmd_virtio_uio
endif
endif

ifeq ($(CONFIG_RTE_LIBRTE_I40E_PMD),y)
LIBS += -lrte_pmd_i40e
endif

ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_PMD),y)
LIBS += -lrte_pmd_ixgbe
endif

ifeq ($(CONFIG_RTE_LIBRTE_E1000_PMD),y)
LIBS += -lrte_pmd_e1000
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_RING),y)
LIBS += -lrte_pmd_ring
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_PCAP),y)
LIBS += -lrte_pmd_pcap -lpcap
endif

endif # Plugins

LIBS += $(EXECENV_LDLIBS)

LIBS += -Wl,--end-group

LIBS += -Wl,--no-whole-archive

endif # ifeq ($(NO_AUTOLIBS),)

LIBS += $(CPU_LDLIBS)

# Merge and rename flags
CXXFLAGS := $(CXXFLAGS) $(CFLAGS) $(EXTRA_CFLAGS)

include $(RTE_SDK)/mk/internal/rte.build-pre.mk

LDFLAGS += $(EXTRA_LDFLAGS)

override LDFLAGS := $(call linkerprefix,$(LDFLAGS))

# Restore previous C flags
CFLAGS := $(DPDK_OLD_CFLAGS)
