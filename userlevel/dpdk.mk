ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif
ifeq ($(RTE_TARGET),)
$(error "Please define RTE_TARGET environment variable")
endif

# This file more or less copies rte.app.sdk, but we cannot use directly DPDK's one because its heavy modifications on the compile variables would break Click build process

# Save C flags
DPDK_OLD_CFLAGS := $(CFLAGS)
DPDK_OLD_CXXFLAGS := $(CXXFLAGS)
DPDK_OLD_LDFLAGS := $(LDFLAGS)

include $(RTE_SDK)/mk/rte.vars.mk

DPDK_INCLUDE=$(RTE_SDK)/include

# default path for libs
_LDLIBS-y += -L$(RTE_SDK_BIN)/lib

#
# Order is important: from higher level to lower level
#

# Link only the libraries used in the application
LDFLAGS += --as-needed

_LDLIBS-y += -Wl,--whole-archive

_LDLIBS-$(CONFIG_RTE_BUILD_COMBINE_LIBS)    += -l$(RTE_LIBNAME)

ifneq ($(CONFIG_RTE_BUILD_COMBINE_LIBS),y)

_LDLIBS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR)    += -lrte_distributor
_LDLIBS-$(CONFIG_RTE_LIBRTE_REORDER)        += -lrte_reorder

ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_KNI)            += -lrte_kni
_LDLIBS-$(CONFIG_RTE_LIBRTE_IVSHMEM)        += -lrte_ivshmem
endif

_LDLIBS-$(CONFIG_RTE_LIBRTE_PIPELINE)       += -lrte_pipeline
_LDLIBS-$(CONFIG_RTE_LIBRTE_TABLE)          += -lrte_table
_LDLIBS-$(CONFIG_RTE_LIBRTE_PORT)           += -lrte_port
_LDLIBS-$(CONFIG_RTE_LIBRTE_PDUMP)          += -lrte_pdump
_LDLIBS-$(CONFIG_RTE_LIBRTE_IP_FRAG)        += -lrte_ip_frag
_LDLIBS-$(CONFIG_RTE_LIBRTE_TIMER)          += -lrte_timer
_LDLIBS-$(CONFIG_RTE_LIBRTE_HASH)           += -lrte_hash
_LDLIBS-$(CONFIG_RTE_LIBRTE_JOBSTATS)       += -lrte_jobstats
_LDLIBS-$(CONFIG_RTE_LIBRTE_LPM)            += -lrte_lpm
_LDLIBS-$(CONFIG_RTE_LIBRTE_POWER)          += -lrte_power
_LDLIBS-$(CONFIG_RTE_LIBRTE_ACL)            += -lrte_acl
_LDLIBS-$(CONFIG_RTE_LIBRTE_METER)          += -lrte_meter

_LDLIBS-$(CONFIG_RTE_LIBRTE_SCHED)          += -lrte_sched
_LDLIBS-$(CONFIG_RTE_LIBRTE_SCHED)          += -lm
_LDLIBS-$(CONFIG_RTE_LIBRTE_SCHED)          += -lrt

_LDLIBS-$(CONFIG_RTE_LIBRTE_VHOST)          += -lrte_vhost

endif # ! CONFIG_RTE_BUILD_COMBINE_LIBS

_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_PCAP)       += -lpcap

_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SZEDATA2)   += -lsze2

ifeq ($(CONFIG_RTE_LIBRTE_VHOST_NUMA),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_VHOST)          += -lnuma
endif

ifeq ($(CONFIG_RTE_LIBRTE_VHOST_USER),n)
_LDLIBS-$(CONFIG_RTE_LIBRTE_VHOST)          += -lfuse
endif

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),n)
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX4_PMD)       += -libverbs
endif # ! CONFIG_RTE_BUILD_SHARED_LIBS

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),n)
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += -libverbs
endif # ! CONFIG_RTE_BUILD_SHARED_LIBS

_LDLIBS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD)      += -lz

_LDLIBS-y += -Wl,--start-group

ifneq ($(CONFIG_RTE_BUILD_COMBINE_LIBS),y)

_LDLIBS-$(CONFIG_RTE_LIBRTE_KVARGS)         += -lrte_kvargs
_LDLIBS-$(CONFIG_RTE_LIBRTE_MBUF)           += -lrte_mbuf
_LDLIBS-$(CONFIG_RTE_LIBRTE_MBUF_OFFLOAD)   += -lrte_mbuf_offload
_LDLIBS-$(CONFIG_RTE_LIBRTE_IP_FRAG)        += -lrte_ip_frag
_LDLIBS-$(CONFIG_RTE_LIBRTE_ETHER)          += -lethdev
#Following line is kept for backward compatibility with DPDK<=2.0.0
_LDLIBS-$(CONFIG_RTE_LIBRTE_MALLOC)         += -lrte_malloc
_LDLIBS-$(CONFIG_RTE_LIBRTE_CRYPTODEV)      += -lrte_cryptodev
_LDLIBS-$(CONFIG_RTE_LIBRTE_MEMPOOL)        += -lrte_mempool
_LDLIBS-$(CONFIG_RTE_LIBRTE_RING)           += -lrte_ring
_LDLIBS-$(CONFIG_RTE_LIBRTE_EAL)            += -lrte_eal
_LDLIBS-$(CONFIG_RTE_LIBRTE_CMDLINE)        += -lrte_cmdline
_LDLIBS-$(CONFIG_RTE_LIBRTE_CFGFILE)        += -lrte_cfgfile


_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BOND)       += -lrte_pmd_bond
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_XENVIRT)    += -lrte_pmd_xenvirt
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_XENVIRT)    += -lxenstore

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),n)
# plugins (link only if static libraries)

ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] || [ \( $(RTE_VER_MAJOR) -ge 2 \) -a \( $(RTE_VER_MINOR) -ge 1 \) ] && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD)     += -lrte_pmd_virtio
else
_LDLIBS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD)     += -lrte_pmd_virtio_uio
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD)      += -lrte_pmd_bnx2x -lz
_LDLIBS-$(CONFIG_RTE_LIBRTE_BNXT_PMD)       += -lrte_pmd_bnxt
_LDLIBS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD)      += -lrte_pmd_cxgbe
_LDLIBS-$(CONFIG_RTE_LIBRTE_ENIC_PMD)       += -lrte_pmd_enic
_LDLIBS-$(CONFIG_RTE_LIBRTE_I40E_PMD)       += -lrte_pmd_i40e
_LDLIBS-$(CONFIG_RTE_LIBRTE_FM10K_PMD)      += -lrte_pmd_fm10k
_LDLIBS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD)      += -lrte_pmd_ixgbe
_LDLIBS-$(CONFIG_RTE_LIBRTE_E1000_PMD)      += -lrte_pmd_e1000
_LDLIBS-$(CONFIG_RTE_LIBRTE_ENA_PMD)        += -lrte_pmd_ena
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX4_PMD)       += -lrte_pmd_mlx4
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += -lrte_pmd_mlx5
_LDLIBS-$(CONFIG_RTE_LIBRTE_MPIPE_PMD)      += -lrte_pmd_mpipe -lgxio
_LDLIBS-$(CONFIG_RTE_LIBRTE_QEDE_PMD)       += -lrte_pmd_qede -lz
_LDLIBS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += -lrte_pmd_thunderx_nicvf -lm
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SZEDATA2)   += -lrte_pmd_szedata2
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_RING)       += -lrte_pmd_ring
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_PCAP)       += -lrte_pmd_pcap
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AF_PACKET)  += -lrte_pmd_af_packet
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_NULL)       += -lrte_pmd_null
_LDLIBS-$(CONFIG_RTE_LIBRTE_VMXNET3_PMD)    += -lrte_pmd_vmxnet3_uio


ifeq ($(CONFIG_RTE_LIBRTE_CRYPTODEV),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_MB)   += -lrte_pmd_aesni_mb
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_MB)   += -L$(AESNI_MULTI_BUFFER_LIB_PATH) -lIPSec_MB
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_GCM)  += -lrte_pmd_aesni_gcm -lcrypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_GCM)  += -L$(AESNI_MULTI_BUFFER_LIB_PATH) -lIPSec_MB
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_NULL_CRYPTO) += -lrte_pmd_null_crypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_QAT)        += -lrte_pmd_qat -lcrypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G)     += -lrte_pmd_snow3g
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G)     += -L$(LIBSSO_SNOW3G_PATH)/build -lsso_snow3g
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI)     += -lrte_pmd_kasumi
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI)     += -L$(LIBSSO_KASUMI_PATH)/build -lsso_kasumi
endif # CONFIG_RTE_LIBRTE_CRYPTODEV

endif # ! $(CONFIG_RTE_BUILD_SHARED_LIB)

endif # ! CONFIG_RTE_BUILD_COMBINE_LIBS

_LDLIBS-y += $(EXECENV_LDLIBS)
_LDLIBS-y += -Wl,--end-group
_LDLIBS-y += -Wl,--no-whole-archive

DPDK_LIB=$(_LDLIBS-y) $(CPU_LDLIBS) $(EXTRA_LDLIBS)

# Eliminate duplicates without sorting
DPDK_LIB := $(shell echo $(DPDK_LIB) | \
    awk '{for (i = 1; i <= NF; i++) { if (!seen[$$i]++) print $$i }}')


RTE_SDK_FULL=`readlink -f $RTE_SDK`

CFLAGS += -I$(DPDK_INCLUDE)

# Merge and rename flags
CXXFLAGS := $(CXXFLAGS) $(CFLAGS) $(EXTRA_CFLAGS)

include $(RTE_SDK)/mk/internal/rte.build-pre.mk


override LDFLAGS := $(DPDK_OLD_LDFLAGS) $(DPDK_LIB)
