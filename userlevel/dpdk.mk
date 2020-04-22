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

_LDLIBS-$(CONFIG_RTE_LIBRTE_REORDER)        += -lrte_reorder

ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_KNI)            += -lrte_kni
_LDLIBS-$(CONFIG_RTE_LIBRTE_IVSHMEM)        += -lrte_ivshmem
endif

OTX-CPT-y := $(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO)
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
OTX-CPT-y += $(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO)
endif
ifeq ($(findstring y,$(OTX-CPT-y)),y)
_LDLIBS-y += -lrte_common_cpt
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF)$(CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL)$(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 18 ] && [ "$(RTE_VER_MONTH)" -ge 05 ] ) || [ $(RTE_VER_YEAR) -ge 19 ] ) && echo true),yytrue)
	_LDLIBS-y += -lrte_common_octeontx
endif
OCTEONTX2-y := $(CONFIG_RTE_LIBRTE_OCTEONTX2_MEMPOOL)
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
OCTEONTX2-y += $(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO)
endif
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 08 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
OCTEONTX2-y += $(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV)
OCTEONTX2-y += $(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_DMA_RAWDEV)
OCTEONTX2-y += $(CONFIG_RTE_LIBRTE_OCTEONTX2_PMD)
endif
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
OCTEONTX2-y += $(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EP_RAWDEV)
endif
ifeq ($(findstring y,$(OCTEONTX2-y)),y)
	_LDLIBS-y += -lrte_common_octeontx2
endif

_LDLIBS-$(CONFIG_RTE_LIBRTE_FLOW_CLASSIFY)  += -lrte_flow_classify
_LDLIBS-$(CONFIG_RTE_LIBRTE_PIPELINE)       += -lrte_pipeline
_LDLIBS-$(CONFIG_RTE_LIBRTE_TABLE)          += -lrte_table
_LDLIBS-$(CONFIG_RTE_LIBRTE_PORT)           += -lrte_port
_LDLIBS-$(CONFIG_RTE_LIBRTE_PDUMP)          += -lrte_pdump
_LDLIBS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR)    += -lrte_distributor
_LDLIBS-$(CONFIG_RTE_LIBRTE_IP_FRAG)        += -lrte_ip_frag
_LDLIBS-$(CONFIG_RTE_LIBRTE_METER)          += -lrte_meter
_LDLIBS-$(CONFIG_RTE_LIBRTE_LPM)            += -lrte_lpm
_LDLIBS-$(CONFIG_RTE_LIBRTE_ACL)            += -lrte_acl
_LDLIBS-$(CONFIG_RTE_LIBRTE_TELEMETRY)      += -lrte_telemetry -ljansson
_LDLIBS-$(CONFIG_RTE_LIBRTE_JOBSTATS)       += -lrte_jobstats
_LDLIBS-$(CONFIG_RTE_LIBRTE_METRICS)        += -lrte_metrics
_LDLIBS-$(CONFIG_RTE_LIBRTE_BITRATE)        += -lrte_bitratestats
_LDLIBS-$(CONFIG_RTE_LIBRTE_LATENCY_STATS)  += -lrte_latencystats
_LDLIBS-$(CONFIG_RTE_LIBRTE_POWER)          += -lrte_power
_LDLIBS-$(CONFIG_RTE_LIBRTE_EFD)            += -lrte_efd
_LDLIBS-$(CONFIG_RTE_LIBRTE_BPF)            += -lrte_bpf
ifeq ($(CONFIG_RTE_LIBRTE_BPF_ELF),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_BPF)            += -lelf
endif

_LDLIBS-$(CONFIG_RTE_LIBRTE_IPSEC)          += -lrte_ipsec
_LDLIBS-$(CONFIG_RTE_LIBRTE_CFGFILE)        += -lrte_cfgfile
_LDLIBS-$(CONFIG_RTE_LIBRTE_GRO)            += -lrte_gro
_LDLIBS-$(CONFIG_RTE_LIBRTE_GSO)            += -lrte_gso
_LDLIBS-$(CONFIG_RTE_LIBRTE_HASH)           += -lrte_hash
_LDLIBS-$(CONFIG_RTE_LIBRTE_MEMBER)         += -lrte_member
ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP)$(CONFIG_RTE_USE_LIBBSD),yy)
_LDLIBS-$(CONFIG_RTE_LIBRTE_EAL)            += -lbsd
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_SCHED)          += -lrte_sched
_LDLIBS-$(CONFIG_RTE_LIBRTE_SCHED)          += -lm
_LDLIBS-$(CONFIG_RTE_LIBRTE_SCHED)          += -lrt
_LDLIBS-$(CONFIG_RTE_LIBRTE_MEMBER)         += -lm
_LDLIBS-$(CONFIG_RTE_LIBRTE_METER)          += -lm

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

_LDLIBS-y += -Wl,--start-group

ifneq ($(CONFIG_RTE_BUILD_COMBINE_LIBS),y)

_LDLIBS-$(CONFIG_RTE_LIBRTE_KVARGS)         += -lrte_kvargs
_LDLIBS-$(CONFIG_RTE_LIBRTE_MBUF)           += -lrte_mbuf
_LDLIBS-$(CONFIG_RTE_LIBRTE_MBUF_OFFLOAD)   += -lrte_mbuf_offload

ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 16 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 17 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_ETHER)          += -lrte_ethdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_BBDEV)          += -lrte_bbdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_NET)            += -lrte_net
else
_LDLIBS-$(CONFIG_RTE_LIBRTE_ETHER)          += -lethdev
endif

#Following line is kept for backward compatibility with DPDK<=2.0.0
_LDLIBS-$(CONFIG_RTE_LIBRTE_MALLOC)         += -lrte_malloc
_LDLIBS-$(CONFIG_RTE_LIBRTE_CRYPTODEV)      += -lrte_cryptodev
_LDLIBS-$(CONFIG_RTE_LIBRTE_SECURITY)       += -lrte_security
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
	ifeq ($(CONFIG_RTE_LIBRTE_SECURITY),y)
		_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_CAAM_JR)   += -lrte_pmd_caam_jr
	endif
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_COMPRESSDEV)    += -lrte_compressdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_EVENTDEV)       += -lrte_eventdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_RAWDEV)         += -lrte_rawdev
ifeq ($(CONFIG_RTE_LIBRTE_RAWDEV),y)
	ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 08 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
		_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SKELETON_RAWDEV) += -lrte_rawdev_skeleton
	else
		_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SKELETON_RAWDEV) += -lrte_pmd_skeleton_rawdev
	endif

	ifeq ($(CONFIG_RTE_EAL_VFIO)$(CONFIG_RTE_LIBRTE_FSLMC_BUS),yy)
		ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 08 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
			_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_CMDIF_RAWDEV) += -lrte_rawdev_dpaa2_cmdif
			_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_QDMA_RAWDEV)  += -lrte_rawdev_dpaa2_qdma
		else
			_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_CMDIF_RAWDEV) += -lrte_pmd_dpaa2_cmdif
			_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_QDMA_RAWDEV)  += -lrte_pmd_dpaa2_qdma
		endif
	endif # CONFIG_RTE_LIBRTE_FSLMC_BUS
	_LDLIBS-$(CONFIG_RTE_LIBRTE_IFPGA_BUS) += -lrte_bus_ifpga
	ifeq ($(CONFIG_RTE_LIBRTE_IFPGA_BUS),y)
		ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 08 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
			_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_IFPGA_RAWDEV) += -lrte_rawdev_ifpga
			_LDLIBS-$(CONFIG_RTE_LIBRTE_IPN3KE_PMD)       += -lrte_pmd_ipn3ke
		else
			_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_IFPGA_RAWDEV) += -lrte_pmd_ifpga_rawdev
		endif
	endif # CONFIG_RTE_LIBRTE_IFPGA_BUS
endif # CONFIG_RTE_LIBRTE_RAWDEV
_LDLIBS-$(CONFIG_RTE_LIBRTE_TIMER)          += -lrte_timer
_LDLIBS-$(CONFIG_RTE_LIBRTE_MEMPOOL)        += -lrte_mempool
_LDLIBS-$(CONFIG_RTE_LIBRTE_STACK)          += -lrte_stack
_LDLIBS-$(CONFIG_RTE_DRIVER_MEMPOOL_RING)   += -lrte_mempool_ring
_LDLIBS-$(CONFIG_RTE_LIBRTE_MEMPOOL_RING)   += -lrte_mempool_ring
_LDLIBS-$(CONFIG_RTE_LIBRTE_RING)           += -lrte_ring
_LDLIBS-$(CONFIG_RTE_LIBRTE_PCI)            += -lrte_pci
_LDLIBS-$(CONFIG_RTE_LIBRTE_EAL)            += -lrte_eal
_LDLIBS-$(CONFIG_RTE_LIBRTE_CMDLINE)        += -lrte_cmdline
ifeq ($(CONFIG_RTE_LIBRTE_FSLMC_BUS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_COMMON_DPAAX)   += -lrte_common_dpaax
endif
ifeq ($(CONFIG_RTE_LIBRTE_DPAA_BUS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_COMMON_DPAAX)   += -lrte_common_dpaax
endif
ifeq ($(CONFIG_RTE_LIBRTE_MVPP2_PMD),y)
_LDLIBS-y += -lrte_common_mvep -L$(LIBMUSDK_PATH)/lib -lmusdk
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_PCI_BUS)        += -lrte_bus_pci
_LDLIBS-$(CONFIG_RTE_LIBRTE_VDEV_BUS)       += -lrte_bus_vdev

_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BOND)       += -lrte_pmd_bond
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_XENVIRT)    += -lrte_pmd_xenvirt
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_XENVIRT)    += -lxenstore

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),n)
# plugins (link only if static libraries)
_LDLIBS-$(CONFIG_RTE_DRIVER_MEMPOOL_RING)   += -lrte_mempool_ring
_LDLIBS-$(CONFIG_RTE_DRIVER_MEMPOOL_BUCKET) += -lrte_mempool_bucket
_LDLIBS-$(CONFIG_RTE_DRIVER_MEMPOOL_STACK)  += -lrte_mempool_stack

ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] || [ \( $(RTE_VER_MAJOR) -ge 2 \) -a \( $(RTE_VER_MINOR) -ge 1 \) ] && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD)     += -lrte_pmd_virtio
else
_LDLIBS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD)     += -lrte_pmd_virtio_uio
endif
ifeq ($(CONFIG_RTE_LIBRTE_VHOST),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_VHOST)      += -lrte_pmd_vhost
ifeq ($(CONFIG_RTE_EAL_VFIO),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_IFCVF_VDPA_PMD) += -lrte_ifcvf_vdpa
_LDLIBS-$(CONFIG_RTE_LIBRTE_IFC_PMD)        += -lrte_pmd_ifc
endif # $(CONFIG_RTE_EAL_VFIO)
endif # $(CONFIG_RTE_LIBRTE_VHOST)
_LDLIBS-$(CONFIG_RTE_LIBRTE_AXGBE_PMD)      += -lrte_pmd_axgbe
_LDLIBS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD)      += -lrte_pmd_bnx2x
_NEED_LZ-$(CONFIG_RTE_LIBRTE_BNX2X_PMD)      = -lz
_LDLIBS-$(CONFIG_RTE_LIBRTE_BNXT_PMD)       += -lrte_pmd_bnxt
_LDLIBS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD)      += -lrte_pmd_cxgbe
ifeq ($(CONFIG_RTE_LIBRTE_DPAA_BUS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA_BUS)       += -lrte_bus_dpaa
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA_MEMPOOL)   += -lrte_mempool_dpaa
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA_PMD)       += -lrte_pmd_dpaa
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA_SEC)   += -lrte_pmd_dpaa_sec
endif
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_VIRTIO_CRYPTO) += -lrte_pmd_virtio_crypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD)      += -lrte_pmd_dpaa2
_LDLIBS-$(CONFIG_RTE_LIBRTE_ENIC_PMD)       += -lrte_pmd_enic
_LDLIBS-$(CONFIG_RTE_LIBRTE_FM10K_PMD)      += -lrte_pmd_fm10k
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE)   += -lrte_pmd_failsafe
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 05 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_HINIC_PMD)      += -lrte_pmd_hinic
endif
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_HNS3_PMD)       += -lrte_pmd_hns3
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_I40E_PMD)       += -lrte_pmd_i40e
_LDLIBS-$(CONFIG_RTE_LIBRTE_IAVF_PMD)       += -lrte_pmd_iavf
_LDLIBS-$(CONFIG_RTE_LIBRTE_ICE_PMD)        += -lrte_pmd_ice
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
IAVF-y := $(CONFIG_RTE_LIBRTE_IAVF_PMD)
ifeq ($(findstring y,$(IAVF-y)),y)
	_LDLIBS-y += -lrte_common_iavf
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_IONIC_PMD)      += -lrte_pmd_ionic
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_IONIC_PMD)      += -lrte_pmd_ionic
_LDLIBS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD)      += -lrte_pmd_ixgbe
ifeq ($(CONFIG_RTE_LIBRTE_KNI),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_KNI)        += -lrte_pmd_kni
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_LIO_PMD)        += -lrte_pmd_lio
_LDLIBS-$(CONFIG_RTE_LIBRTE_E1000_PMD)      += -lrte_pmd_e1000
_LDLIBS-$(CONFIG_RTE_LIBRTE_ENA_PMD)        += -lrte_pmd_ena
_LDLIBS-$(CONFIG_RTE_LIBRTE_ENETC_PMD)      += -lrte_pmd_enetc
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX4_PMD)       += -lrte_pmd_mlx4
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
ifeq ($(findstring y,$(CONFIG_RTE_LIBRTE_MLX5_PMD)$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD)),y)
_LDLIBS-y                                   += -lrte_common_mlx5
endif
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += -lrte_pmd_mlx5
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD)  += -lrte_pmd_mlx5_vdpa
endif
ifeq ($(CONFIG_RTE_IBVERBS_LINK_DLOPEN),y)
	ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
		_LDLIBS-y                                   += -ldl
	else
		_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX4_PMD)       += -ldl
		_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += -ldl
	endif
else ifeq ($(CONFIG_RTE_IBVERBS_LINK_STATIC),y)
	LIBS_IBVERBS_STATIC = $(shell $(RTE_SDK)/buildtools/options-ibverbs-static.sh)
	ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
		_LDLIBS-y                                   += $(LIBS_IBVERBS_STATIC)
	else
		_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX4_PMD)       += $(LIBS_IBVERBS_STATIC)
		_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += $(LIBS_IBVERBS_STATIC)
	endif
else
	_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX4_PMD)       += -libverbs -lmlx4
	ifeq ($(findstring y,$(CONFIG_RTE_LIBRTE_MLX5_PMD)$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD)),y)
		_LDLIBS-y                                   += -libverbs -lmlx5
	else
		_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += -libverbs -lmlx5
	endif
endif

# DPDK 17.11 or beyond requires additional libraries for Mellanox NICs
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 17 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 18 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX4_PMD)       += -lmlx4
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += -lmlx5
endif
# DPDK 18.08 or beyond requires libmnl library for MLX5 NICs
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 18 ] && [ "$(RTE_VER_MONTH)" -ge 08 ] ) || [ $(RTE_VER_YEAR) -ge 19 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += -lmnl
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_MVPP2_PMD)      += -lrte_pmd_mvpp2 -L$(LIBMUSDK_PATH)/lib -lmusdk
_LDLIBS-$(CONFIG_RTE_LIBRTE_MRVL_PMD)       += -lrte_pmd_mrvl -L$(LIBMUSDK_PATH)/lib -lmusdk
_LDLIBS-$(CONFIG_RTE_LIBRTE_NFP_PMD)        += -lrte_pmd_nfp
_LDLIBS-$(CONFIG_RTE_LIBRTE_MPIPE_PMD)      += -lrte_pmd_mpipe -lgxio
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PFE_PMD)        += -lrte_pmd_pfe
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_QEDE_PMD)       += -lrte_pmd_qede
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ \( $(RTE_VER_YEAR) -gt 16 \) -o \( $(RTE_VER_MINOR) -ge 11 \) ] && echo true),true)
_NEED_LZ-$(CONFIG_RTE_LIBRTE_QEDE_PMD)       = -lz
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += -lrte_pmd_thunderx_nicvf -lm
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SZEDATA2)   += -lrte_pmd_szedata2
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_RING)       += -lrte_pmd_ring
ifeq ($(CONFIG_RTE_LIBRTE_SCHED),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC)    += -lrte_pmd_softnic
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD)    += -lrte_pmd_sfc_efx
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_TAP)        += -lrte_pmd_tap
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_PCAP)       += -lrte_pmd_pcap
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AF_PACKET)  += -lrte_pmd_af_packet
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AF_XDP)     += -lrte_pmd_af_xdp -lbpf
_LDLIBS-$(CONFIG_RTE_LIBRTE_ARK_PMD)        += -lrte_pmd_ark
_LDLIBS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD)   += -lrte_pmd_atlantic
_LDLIBS-$(CONFIG_RTE_LIBRTE_AVF_PMD)        += -lrte_pmd_avf
_LDLIBS-$(CONFIG_RTE_LIBRTE_IAVF_PMD)       += -lrte_pmd_iavf
_LDLIBS-$(CONFIG_RTE_LIBRTE_AVP_PMD)        += -lrte_pmd_avp
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_NULL)       += -lrte_pmd_null
_LDLIBS-$(CONFIG_RTE_LIBRTE_VMXNET3_PMD)    += -lrte_pmd_vmxnet3_uio

_LDLIBS-$(CONFIG_RTE_LIBRTE_VMBUS)          += -lrte_bus_vmbus
_LDLIBS-$(CONFIG_RTE_LIBRTE_NETVSC_PMD)     += -lrte_pmd_netvsc

ifeq ($(CONFIG_RTE_LIBRTE_CRYPTODEV),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_MB)   += -lrte_pmd_aesni_mb
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_MB)   += -L$(AESNI_MULTI_BUFFER_LIB_PATH) -lIPSec_MB
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_GCM)  += -lrte_pmd_aesni_gcm -lcrypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_GCM)  += -L$(AESNI_MULTI_BUFFER_LIB_PATH) -lIPSec_MB
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_CCP)        += -lrte_pmd_ccp -lcrypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_NULL_CRYPTO) += -lrte_pmd_null_crypto

ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 18 ] && [ "$(RTE_VER_MONTH)" -ge 08 ] ) || [ $(RTE_VER_YEAR) -ge 19 ] ) && echo true),true)
ifeq ($(CONFIG_RTE_LIBRTE_PMD_QAT),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_QAT_SYM)    += -lrte_pmd_qat -lcrypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_QAT_ASYM)   += -lrte_pmd_qat -lcrypto
endif # CONFIG_RTE_LIBRTE_PMD_QAT
else
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_QAT)        += -lrte_pmd_qat -lcrypto
endif

_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G)           += -lrte_pmd_snow3g
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G)      += -lIPSec_MB
else
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G)           += -L$(LIBSSO_SNOW3G_PATH)/build -lsso_snow3g
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI)           += -lrte_pmd_kasumi
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI)      += -lIPSec_MB
else
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI)           += -L$(LIBSSO_KASUMI_PATH)/build -lsso_kasumi
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ZUC)              += -lrte_pmd_zuc
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ZUC)         += -lIPSec_MB
else
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ZUC)              += -L$(LIBSSO_ZUC_PATH)/build -lsso_zuc
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO)     += -lrte_pmd_armv8
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && [ "$(RTE_VER_YEAR)" -ge 20 ] && echo true),true)
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO)    += -L$(ARMV8_CRYPTO_LIB_PATH) -lAArch64crypto
else
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO)     += -L$(ARMV8_CRYPTO_LIB_PATH) -larmv8_crypto
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_MRVL_CRYPTO)      += -L$(LIBMUSDK_PATH)/lib -lrte_pmd_mrvl_crypto -lmusdk
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_MVSAM_CRYPTO)     += -L$(LIBMUSDK_PATH)/lib -lrte_pmd_mvsam_crypto -lmusdk
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_NITROX)           += -lrte_pmd_nitrox
endif
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 18 ] && [ "$(RTE_VER_MONTH)" -ge 08 ] ) || [ $(RTE_VER_YEAR) -ge 19 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO) += -lrte_pmd_octeontx_crypto
endif
ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 19 ] && [ "$(RTE_VER_MONTH)" -ge 11 ] ) || [ $(RTE_VER_YEAR) -ge 20 ] ) && echo true),true)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO) += -lrte_pmd_octeontx2_crypto
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += -lrte_pmd_crypto_scheduler
ifeq ($(CONFIG_RTE_LIBRTE_FSLMC_BUS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC)   += -lrte_pmd_dpaa2_sec
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC)   += -lrte_mempool_dpaa2
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC)   += -lrte_bus_fslmc
endif # CONFIG_RTE_LIBRTE_FSLMC_BUS
endif # CONFIG_RTE_LIBRTE_CRYPTODEV

ifeq ($(CONFIG_RTE_LIBRTE_COMPRESSDEV),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ISAL) += -lrte_pmd_isal_comp
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ISAL) += -lisal
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_ZIPVF) += -lrte_pmd_octeontx_zip
# Link QAT driver if it has not been linked yet
ifeq ($(CONFIG_RTE_LIBRTE_PMD_QAT_SYM),n)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_QAT)  += -lrte_pmd_qat
endif # CONFIG_RTE_LIBRTE_PMD_QAT_SYM
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ZLIB) += -lrte_pmd_zlib
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ZLIB) += -lz
endif # CONFIG_RTE_LIBRTE_COMPRESSDEV


ifeq ($(CONFIG_RTE_LIBRTE_EVENTDEV),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SKELETON_EVENTDEV)  += -lrte_pmd_skeleton_event
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV)        += -lrte_pmd_sw_event
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF)     += -lrte_pmd_octeontx_ssovf
ifeq ($(CONFIG_RTE_LIBRTE_DPAA_BUS),y)
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA_EVENTDEV)  += -lrte_pmd_dpaa_event
endif # CONFIG_RTE_LIBRTE_DPAA_BUS
ifeq ($(CONFIG_RTE_EAL_VFIO)$(CONFIG_RTE_LIBRTE_FSLMC_BUS),yy)
	_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_EVENTDEV) += -lrte_pmd_dpaa2_event
endif # CONFIG_RTE_LIBRTE_FSLMC_BUS

_LDLIBS-$(CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL)       += -lrte_mempool_octeontx
_LDLIBS-$(CONFIG_RTE_LIBRTE_OCTEONTX_PMD)           += -lrte_pmd_octeontx
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV)      += -lrte_pmd_opdl_event
endif # CONFIG_RTE_LIBRTE_EVENTDEV

ifeq ($(CONFIG_RTE_LIBRTE_DPAA2_PMD),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD)              += -lrte_bus_fslmc
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD)              += -lrte_mempool_dpaa2
endif # CONFIG_RTE_LIBRTE_DPAA2_PMD

ifeq ($(CONFIG_RTE_LIBRTE_COMMON_DPAAX),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_COMMON_DPAAX)   += -lrte_common_dpaax
endif # CONFIG_RTE_LIBRTE_COMMON_DPAAX

endif # ! $(CONFIG_RTE_BUILD_SHARED_LIB)

endif # ! CONFIG_RTE_BUILD_COMBINE_LIBS

_LDLIBS-y += $(_NEED_LZ-y)
_LDLIBS-y += $(EXECENV_LDLIBS)
_LDLIBS-y += -Wl,--end-group
_LDLIBS-y += -Wl,--no-whole-archive

DPDK_LIBS=$(_LDLIBS-y) $(CPU_LDLIBS) $(EXTRA_LDLIBS)

# Eliminate duplicates without sorting
DPDK_LIBS := $(shell echo $(DPDK_LIBS) | \
    awk '{for (i = 1; i <= NF; i++) { if (!seen[$$i]++) print $$i }}')

RTE_SDK_FULL=`readlink -f $RTE_SDK`

# Merge and rename flags
CXXFLAGS := $(CXXFLAGS) $(CFLAGS) $(EXTRA_CFLAGS)

include $(RTE_SDK)/mk/internal/rte.build-pre.mk

ifeq ($(shell [ -n "$(RTE_VER_YEAR)" ] && ( ( [ "$(RTE_VER_YEAR)" -ge 16 ] && [ "$(RTE_VER_MONTH)" -ge 07 ] ) || [ $(RTE_VER_YEAR) -ge 17 ] ) && echo true),true)
	override LDFLAGS := $(DPDK_OLD_LDFLAGS) $(DPDK_LIBS)
else
	override LDFLAGS := $(DPDK_OLD_LDFLAGS)
endif
