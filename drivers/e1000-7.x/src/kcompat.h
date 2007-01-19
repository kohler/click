/*******************************************************************************

  Intel PRO/1000 Linux driver
  Copyright(c) 1999 - 2006 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _KCOMPAT_H_
#define _KCOMPAT_H_

#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/io.h>

/* NAPI enable/disable flags here */

#ifdef _E1000_H_
#ifdef CONFIG_E1000_NAPI
#define NAPI
#endif
#ifdef E1000_NAPI
#undef NAPI
#define NAPI
#endif
#ifdef E1000_NO_NAPI
#undef NAPI
#endif
#endif

#ifdef _IXGB_H_
#ifdef CONFIG_IXGB_NAPI
#define NAPI
#endif
#ifdef IXGB_NAPI
#undef NAPI
#define NAPI
#endif
#ifdef IXGB_NO_NAPI
#undef NAPI
#endif
#endif

/* and finally set defines so that the code sees the changes */
#ifdef NAPI
#ifndef CONFIG_E1000_NAPI
#define CONFIG_E1000_NAPI
#endif
#ifndef CONFIG_IXGB_NAPI
#define CONFIG_IXGB_NAPI
#endif
#else
#undef CONFIG_E1000_NAPI
#undef CONFIG_IXGB_NAPI
#endif

/* packet split disable/enable */
#ifdef DISABLE_PACKET_SPLIT
#undef CONFIG_E1000_DISABLE_PACKET_SPLIT
#define CONFIG_E1000_DISABLE_PACKET_SPLIT
#endif

/* general compatibility flags unclassified per kernel */
#ifdef DISABLE_PCI_MSI
#undef CONFIG_PCI_MSI
#endif

#ifdef DISABLE_PM
#undef CONFIG_PM
#endif

#ifdef DISABLE_NET_POLL_CONTROLLER
#undef CONFIG_NET_POLL_CONTROLLER
#endif

#ifndef PMSG_SUSPEND
#define PMSG_SUSPEND 3
#endif

#ifndef module_param
#define module_param(v,t,p) MODULE_PARM(v, "i");
#endif

#ifndef DMA_64BIT_MASK
#define DMA_64BIT_MASK  0xffffffffffffffffULL
#endif

#ifndef DMA_32BIT_MASK
#define DMA_32BIT_MASK  0x00000000ffffffffULL
#endif

#ifndef PCI_CAP_ID_EXP
#define PCI_CAP_ID_EXP 0x10
#endif

#ifndef mmiowb
#ifdef CONFIG_IA64
#define mmiowb() asm volatile ("mf.a" ::: "memory")
#else
#define mmiowb()
#endif
#endif

#ifndef IRQ_HANDLED
#define irqreturn_t void
#define IRQ_HANDLED
#define IRQ_NONE
#endif

#ifndef SET_NETDEV_DEV
#define SET_NETDEV_DEV(net, pdev)
#endif

#ifndef HAVE_FREE_NETDEV
#define free_netdev(x)	kfree(x)
#endif

#ifdef HAVE_POLL_CONTROLLER
#define CONFIG_NET_POLL_CONTROLLER
#endif

#ifndef NETDEV_TX_OK
#define NETDEV_TX_OK 0
#endif

#ifndef NETDEV_TX_BUSY
#define NETDEV_TX_BUSY 1
#endif

#ifndef NETDEV_TX_LOCKED
#define NETDEV_TX_LOCKED -1
#endif

#ifndef SKB_DATAREF_SHIFT
/* if we do not have the infrastructure to detect if skb_header is cloned
   just return false in all cases */
#define skb_header_cloned(x) 0
#endif

#ifndef NETIF_F_GSO
#define gso_size tso_size
#define gso_segs tso_segs
#endif

#ifndef CHECKSUM_PARTIAL
#define CHECKSUM_PARTIAL CHECKSUM_HW
#define CHECKSUM_COMPLETE CHECKSUM_HW
#endif

#ifndef __read_mostly
#define __read_mostly
#endif

#ifndef HAVE_NETIF_MSG
#define HAVE_NETIF_MSG 1
enum {
	NETIF_MSG_DRV		= 0x0001,
	NETIF_MSG_PROBE		= 0x0002,
	NETIF_MSG_LINK		= 0x0004,
	NETIF_MSG_TIMER		= 0x0008,
	NETIF_MSG_IFDOWN	= 0x0010,
	NETIF_MSG_IFUP		= 0x0020,
	NETIF_MSG_RX_ERR	= 0x0040,
	NETIF_MSG_TX_ERR	= 0x0080,
	NETIF_MSG_TX_QUEUED	= 0x0100,
	NETIF_MSG_INTR		= 0x0200,
	NETIF_MSG_TX_DONE	= 0x0400,
	NETIF_MSG_RX_STATUS	= 0x0800,
	NETIF_MSG_PKTDATA	= 0x1000,
	NETIF_MSG_HW		= 0x2000,
	NETIF_MSG_WOL		= 0x4000,
};

#else
#define NETIF_MSG_HW	0x2000
#define NETIF_MSG_WOL	0x4000
#endif /* HAVE_NETIF_MSG */

#ifndef MII_RESV1
#define MII_RESV1		0x17		/* Reserved...		*/
#endif

#ifndef unlikely
#define unlikely(_x) _x
#define likely(_x) _x
#endif

#ifndef WARN_ON
#define WARN_ON(x)
#endif

#ifndef PCI_DEVICE
#define PCI_DEVICE(vend,dev) \
	.vendor = (vend), .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#endif

/*****************************************************************************/
/* Installations with ethtool version without eeprom, adapter id, or statistics
 * support */

#ifndef ETH_GSTRING_LEN
#define ETH_GSTRING_LEN 32
#endif

#ifndef ETHTOOL_GSTATS
#define ETHTOOL_GSTATS 0x1d
#undef ethtool_drvinfo
#define ethtool_drvinfo k_ethtool_drvinfo
struct k_ethtool_drvinfo {
	uint32_t cmd;
	char	 driver[32];
	char	 version[32];
	char	 fw_version[32];
	char	 bus_info[32];
	char	 reserved1[32];
	char	 reserved2[16];
	uint32_t n_stats;
	uint32_t testinfo_len;
	uint32_t eedump_len;
	uint32_t regdump_len;
};

struct ethtool_stats {
	uint32_t cmd;
	uint32_t n_stats;
	uint64_t data[0];
};
#endif /* ETHTOOL_GSTATS */

#ifndef ETHTOOL_PHYS_ID
#define ETHTOOL_PHYS_ID 0x1c
#endif /* ETHTOOL_PHYS_ID */

#ifndef ETHTOOL_GSTRINGS
#define ETHTOOL_GSTRINGS 0x1b
enum ethtool_stringset {
	ETH_SS_TEST             = 0,
	ETH_SS_STATS,
};
struct ethtool_gstrings {
	u32     cmd;            /* ETHTOOL_GSTRINGS */
	u32     string_set;     /* string set id e.c. ETH_SS_TEST, etc*/
	u32     len;            /* number of strings in the string set */
	u8      data[0];
};
#endif /* ETHTOOL_GSTRINGS */

#ifndef ETHTOOL_TEST
#define ETHTOOL_TEST 0x1a
enum ethtool_test_flags {
	ETH_TEST_FL_OFFLINE	= (1 << 0),
	ETH_TEST_FL_FAILED	= (1 << 1),
};
struct ethtool_test {
	uint32_t cmd;
	uint32_t flags;
	uint32_t reserved;
	uint32_t len;
	uint64_t data[0];
};
#endif /* ETHTOOL_TEST */

#ifndef ETHTOOL_GEEPROM
#define ETHTOOL_GEEPROM 0xb
#undef ETHTOOL_GREGS
struct ethtool_eeprom {
	uint32_t cmd;
	uint32_t magic;
	uint32_t offset;
	uint32_t len;
	uint8_t	 data[0];
};

struct ethtool_value {
	uint32_t cmd;
	uint32_t data;
};
#endif /* ETHTOOL_GEEPROM */

#ifndef ETHTOOL_GLINK
#define ETHTOOL_GLINK 0xa
#endif /* ETHTOOL_GLINK */

#ifndef ETHTOOL_GREGS
#define ETHTOOL_GREGS		0x00000004 /* Get NIC registers */
#define ethtool_regs _kc_ethtool_regs
/* for passing big chunks of data */
struct _kc_ethtool_regs {
	u32	cmd;
	u32	version; /* driver-specific, indicates different chips/revs */
	u32	len; /* bytes */
	u8	data[0];
};
#endif /* ETHTOOL_GREGS */

#ifndef ETHTOOL_GMSGLVL
#define ETHTOOL_GMSGLVL		0x00000007 /* Get driver message level */
#endif
#ifndef ETHTOOL_SMSGLVL
#define ETHTOOL_SMSGLVL		0x00000008 /* Set driver msg level, priv. */
#endif
#ifndef ETHTOOL_NWAY_RST
#define ETHTOOL_NWAY_RST	0x00000009 /* Restart autonegotiation, priv */
#endif
#ifndef ETHTOOL_GLINK
#define ETHTOOL_GLINK		0x0000000a /* Get link status */
#endif
#ifndef ETHTOOL_GEEPROM
#define ETHTOOL_GEEPROM		0x0000000b /* Get EEPROM data */
#endif
#ifndef ETHTOOL_SEEPROM
#define ETHTOOL_SEEPROM		0x0000000c /* Set EEPROM data */
#endif
#ifndef ETHTOOL_GCOALESCE
#define ETHTOOL_GCOALESCE	0x0000000e /* Get coalesce config */
/* for configuring coalescing parameters of chip */
#define ethtool_coalesce _kc_ethtool_coalesce
struct _kc_ethtool_coalesce {
	u32	cmd;	/* ETHTOOL_{G,S}COALESCE */

	/* How many usecs to delay an RX interrupt after
	 * a packet arrives.  If 0, only rx_max_coalesced_frames
	 * is used.
	 */
	u32	rx_coalesce_usecs;

	/* How many packets to delay an RX interrupt after
	 * a packet arrives.  If 0, only rx_coalesce_usecs is
	 * used.  It is illegal to set both usecs and max frames
	 * to zero as this would cause RX interrupts to never be
	 * generated.
	 */
	u32	rx_max_coalesced_frames;

	/* Same as above two parameters, except that these values
	 * apply while an IRQ is being serviced by the host.  Not
	 * all cards support this feature and the values are ignored
	 * in that case.
	 */
	u32	rx_coalesce_usecs_irq;
	u32	rx_max_coalesced_frames_irq;

	/* How many usecs to delay a TX interrupt after
	 * a packet is sent.  If 0, only tx_max_coalesced_frames
	 * is used.
	 */
	u32	tx_coalesce_usecs;

	/* How many packets to delay a TX interrupt after
	 * a packet is sent.  If 0, only tx_coalesce_usecs is
	 * used.  It is illegal to set both usecs and max frames
	 * to zero as this would cause TX interrupts to never be
	 * generated.
	 */
	u32	tx_max_coalesced_frames;

	/* Same as above two parameters, except that these values
	 * apply while an IRQ is being serviced by the host.  Not
	 * all cards support this feature and the values are ignored
	 * in that case.
	 */
	u32	tx_coalesce_usecs_irq;
	u32	tx_max_coalesced_frames_irq;

	/* How many usecs to delay in-memory statistics
	 * block updates.  Some drivers do not have an in-memory
	 * statistic block, and in such cases this value is ignored.
	 * This value must not be zero.
	 */
	u32	stats_block_coalesce_usecs;

	/* Adaptive RX/TX coalescing is an algorithm implemented by
	 * some drivers to improve latency under low packet rates and
	 * improve throughput under high packet rates.  Some drivers
	 * only implement one of RX or TX adaptive coalescing.  Anything
	 * not implemented by the driver causes these values to be
	 * silently ignored.
	 */
	u32	use_adaptive_rx_coalesce;
	u32	use_adaptive_tx_coalesce;

	/* When the packet rate (measured in packets per second)
	 * is below pkt_rate_low, the {rx,tx}_*_low parameters are
	 * used.
	 */
	u32	pkt_rate_low;
	u32	rx_coalesce_usecs_low;
	u32	rx_max_coalesced_frames_low;
	u32	tx_coalesce_usecs_low;
	u32	tx_max_coalesced_frames_low;

	/* When the packet rate is below pkt_rate_high but above
	 * pkt_rate_low (both measured in packets per second) the
	 * normal {rx,tx}_* coalescing parameters are used.
	 */

	/* When the packet rate is (measured in packets per second)
	 * is above pkt_rate_high, the {rx,tx}_*_high parameters are
	 * used.
	 */
	u32	pkt_rate_high;
	u32	rx_coalesce_usecs_high;
	u32	rx_max_coalesced_frames_high;
	u32	tx_coalesce_usecs_high;
	u32	tx_max_coalesced_frames_high;

	/* How often to do adaptive coalescing packet rate sampling,
	 * measured in seconds.  Must not be zero.
	 */
	u32	rate_sample_interval;
};
#endif /* ETHTOOL_GCOALESCE */

#ifndef ETHTOOL_SCOALESCE
#define ETHTOOL_SCOALESCE	0x0000000f /* Set coalesce config. */
#endif
#ifndef ETHTOOL_GRINGPARAM
#define ETHTOOL_GRINGPARAM	0x00000010 /* Get ring parameters */
/* for configuring RX/TX ring parameters */
#define ethtool_ringparam _kc_ethtool_ringparam
struct _kc_ethtool_ringparam {
	u32	cmd;	/* ETHTOOL_{G,S}RINGPARAM */

	/* Read only attributes.  These indicate the maximum number
	 * of pending RX/TX ring entries the driver will allow the
	 * user to set.
	 */
	u32	rx_max_pending;
	u32	rx_mini_max_pending;
	u32	rx_jumbo_max_pending;
	u32	tx_max_pending;

	/* Values changeable by the user.  The valid values are
	 * in the range 1 to the "*_max_pending" counterpart above.
	 */
	u32	rx_pending;
	u32	rx_mini_pending;
	u32	rx_jumbo_pending;
	u32	tx_pending;
};
#endif /* ETHTOOL_GRINGPARAM */

#ifndef ETHTOOL_SRINGPARAM
#define ETHTOOL_SRINGPARAM	0x00000011 /* Set ring parameters, priv. */
#endif
#ifndef ETHTOOL_GPAUSEPARAM
#define ETHTOOL_GPAUSEPARAM	0x00000012 /* Get pause parameters */
/* for configuring link flow control parameters */
#define ethtool_pauseparam _kc_ethtool_pauseparam
struct _kc_ethtool_pauseparam {
	u32	cmd;	/* ETHTOOL_{G,S}PAUSEPARAM */

	/* If the link is being auto-negotiated (via ethtool_cmd.autoneg
	 * being true) the user may set 'autonet' here non-zero to have the
	 * pause parameters be auto-negotiated too.  In such a case, the
	 * {rx,tx}_pause values below determine what capabilities are
	 * advertised.
	 *
	 * If 'autoneg' is zero or the link is not being auto-negotiated,
	 * then {rx,tx}_pause force the driver to use/not-use pause
	 * flow control.
	 */
	u32	autoneg;
	u32	rx_pause;
	u32	tx_pause;
};
#endif /* ETHTOOL_GPAUSEPARAM */

#ifndef ETHTOOL_SPAUSEPARAM
#define ETHTOOL_SPAUSEPARAM	0x00000013 /* Set pause parameters. */
#endif
#ifndef ETHTOOL_GRXCSUM
#define ETHTOOL_GRXCSUM		0x00000014 /* Get RX hw csum enable (ethtool_value) */
#endif
#ifndef ETHTOOL_SRXCSUM
#define ETHTOOL_SRXCSUM		0x00000015 /* Set RX hw csum enable (ethtool_value) */
#endif
#ifndef ETHTOOL_GTXCSUM
#define ETHTOOL_GTXCSUM		0x00000016 /* Get TX hw csum enable (ethtool_value) */
#endif
#ifndef ETHTOOL_STXCSUM
#define ETHTOOL_STXCSUM		0x00000017 /* Set TX hw csum enable (ethtool_value) */
#endif
#ifndef ETHTOOL_GSG
#define ETHTOOL_GSG		0x00000018 /* Get scatter-gather enable
					    * (ethtool_value) */
#endif
#ifndef ETHTOOL_SSG
#define ETHTOOL_SSG		0x00000019 /* Set scatter-gather enable
					    * (ethtool_value). */
#endif
#ifndef ETHTOOL_TEST
#define ETHTOOL_TEST		0x0000001a /* execute NIC self-test, priv. */
#endif
#ifndef ETHTOOL_GSTRINGS
#define ETHTOOL_GSTRINGS	0x0000001b /* get specified string set */
#endif
#ifndef ETHTOOL_PHYS_ID
#define ETHTOOL_PHYS_ID		0x0000001c /* identify the NIC */
#endif
#ifndef ETHTOOL_GSTATS
#define ETHTOOL_GSTATS		0x0000001d /* get NIC-specific statistics */
#endif
#ifndef ETHTOOL_GTSO
#define ETHTOOL_GTSO		0x0000001e /* Get TSO enable (ethtool_value) */
#endif
#ifndef ETHTOOL_STSO
#define ETHTOOL_STSO		0x0000001f /* Set TSO enable (ethtool_value) */
#endif

#ifndef ETHTOOL_BUSINFO_LEN
#define ETHTOOL_BUSINFO_LEN	32
#endif

/*****************************************************************************/
/* 2.4.3 => 2.4.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3) )

/**************************************/
/* PCI DRIVER API */

#ifndef pci_set_dma_mask
#define pci_set_dma_mask _kc_pci_set_dma_mask
extern int _kc_pci_set_dma_mask(struct pci_dev *dev, dma_addr_t mask);
#endif

#ifndef pci_request_regions
#define pci_request_regions _kc_pci_request_regions
extern int _kc_pci_request_regions(struct pci_dev *pdev, char *res_name);
#endif

#ifndef pci_release_regions
#define pci_release_regions _kc_pci_release_regions
extern void _kc_pci_release_regions(struct pci_dev *pdev);
#endif

/**************************************/
/* NETWORK DRIVER API */

#ifndef alloc_etherdev
#define alloc_etherdev _kc_alloc_etherdev
extern struct net_device * _kc_alloc_etherdev(int sizeof_priv);
#endif

#ifndef is_valid_ether_addr
#define is_valid_ether_addr _kc_is_valid_ether_addr
extern int _kc_is_valid_ether_addr(u8 *addr);
#endif

/**************************************/
/* MISCELLANEOUS */

#ifndef INIT_TQUEUE
#define INIT_TQUEUE(_tq, _routine, _data)		\
	do {						\
		INIT_LIST_HEAD(&(_tq)->list);		\
		(_tq)->sync = 0;			\
		(_tq)->routine = _routine;		\
		(_tq)->data = _data;			\
	} while (0)
#endif

#endif /* 2.4.3 => 2.4.0 */

/*****************************************************************************/
/* 2.4.6 => 2.4.3 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,6) )

#ifndef pci_set_power_state
#define pci_set_power_state _kc_pci_set_power_state
extern int _kc_pci_set_power_state(struct pci_dev *dev, int state);
#endif

#ifndef pci_save_state
#define pci_save_state _kc_pci_save_state
extern int _kc_pci_save_state(struct pci_dev *dev, u32 *buffer);
#endif

#ifndef pci_restore_state
#define pci_restore_state _kc_pci_restore_state
extern int _kc_pci_restore_state(struct pci_dev *pdev, u32 *buffer);
#endif

#ifndef pci_enable_wake
#define pci_enable_wake _kc_pci_enable_wake
extern int _kc_pci_enable_wake(struct pci_dev *pdev, u32 state, int enable);
#endif

#ifndef pci_disable_device
#define pci_disable_device _kc_pci_disable_device
extern void _kc_pci_disable_device(struct pci_dev *pdev);
#endif

/* PCI PM entry point syntax changed, so don't support suspend/resume */
#undef CONFIG_PM

#endif /* 2.4.6 => 2.4.3 */

/*****************************************************************************/
/* 2.4.9 => 2.4.6 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,9) )
#ifndef HAVE_PCI_SET_MWI
#define pci_set_mwi(X) pci_write_config_word(X, \
			       PCI_COMMAND, adapter->hw.pci_cmd_word | \
			       PCI_COMMAND_INVALIDATE);
#define pci_clear_mwi(X) pci_write_config_word(X, \
			       PCI_COMMAND, adapter->hw.pci_cmd_word & \
			       ~PCI_COMMAND_INVALIDATE);
#endif
#endif /* 2.4.9 => 2.4.6 */

/*****************************************************************************/
/* 2.4.10 => 2.4.9 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10) )

/**************************************/
/* MODULE API */

#ifndef MODULE_LICENSE
	#define MODULE_LICENSE(X)
#endif

/**************************************/
/* OTHER */

#undef min
#define min(x,y) ({ \
	const typeof(x) _x = (x);	\
	const typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x < _y ? _x : _y; })

#undef max
#define max(x,y) ({ \
	const typeof(x) _x = (x);	\
	const typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x > _y ? _x : _y; })

#ifndef list_for_each_safe
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)
#endif

#endif /* 2.4.10 -> 2.4.6 */


/*****************************************************************************/
/* 2.4.13 => 2.4.10 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,13) )

/**************************************/
/* PCI DMA MAPPING */

#ifndef virt_to_page
	#define virt_to_page(v) (mem_map + (virt_to_phys(v) >> PAGE_SHIFT))
#endif

#ifndef pci_map_page
#define pci_map_page _kc_pci_map_page
extern u64 _kc_pci_map_page(struct pci_dev *dev, struct page *page, unsigned long offset, size_t size, int direction);
#endif

#ifndef pci_unmap_page
#define pci_unmap_page _kc_pci_unmap_page
extern void _kc_pci_unmap_page(struct pci_dev *dev, u64 dma_addr, size_t size, int direction);
#endif

/* pci_set_dma_mask takes dma_addr_t, which is only 32-bits prior to 2.4.13 */

#undef DMA_32BIT_MASK
#define DMA_32BIT_MASK	0xffffffff
#undef DMA_64BIT_MASK
#define DMA_64BIT_MASK	0xffffffff

/**************************************/
/* OTHER */

#ifndef cpu_relax
#define cpu_relax()	rep_nop()
#endif

#endif /* 2.4.13 => 2.4.10 */

/*****************************************************************************/
/* 2.4.17 => 2.4.12 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,17) )

#ifndef __devexit_p
	#define __devexit_p(x) &(x)
#endif

#ifndef VLAN_HLEN
#define VLAN_HLEN 4
#endif

#ifndef VLAN_ETH_HLEN
#define VLAN_ETH_HLEN 18
#endif

#ifndef VLAN_ETH_FRAME_LEN
#define VLAN_ETH_FRAME_LEN 1518
#endif

#endif /* 2.4.17 => 2.4.13 */

/*****************************************************************************/
/* 2.4.20 => 2.4.19 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20) )

/* we won't support NAPI on less than 2.4.20 */
#ifdef NAPI
#undef CONFIG_E100_NAPI
#undef CONFIG_IXGB_NAPI
#endif

#endif /* 2.4.20 => 2.4.19 */
/*****************************************************************************/
/* 2.4.22 => 2.4.17 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22) )
#define pci_name(x)	((x)->slot_name)
#endif

/*****************************************************************************/
/* 2.4.23 => 2.4.22 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,23) )
/*****************************************************************************/
#ifdef NAPI
#ifndef netif_poll_disable
#define netif_poll_disable(x) _kc_netif_poll_disable(x)
static inline void _kc_netif_poll_disable(struct net_device *netdev)
{
	while (test_and_set_bit(__LINK_STATE_RX_SCHED, &netdev->state)) {
		/* No hurry */
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(1);
	}
}
#endif

#ifndef netif_poll_enable
#define netif_poll_enable(x) _kc_netif_poll_enable(x)
static inline void _kc_netif_poll_enable(struct net_device *netdev)
{
	clear_bit(__LINK_STATE_RX_SCHED, &netdev->state);
}
#endif
#endif /* NAPI */
#endif /* 2.4.23 => 2.4.22 */

/*****************************************************************************/
/* 2.6.4 => 2.6.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,25) || \
    ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) && \
      LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4) ) )
#define ETHTOOL_OPS_COMPAT
#endif /* 2.6.4 => 2.6.0 */

/*****************************************************************************/
/* 2.5.71 => 2.4.x */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,5,71) )
#define sk_protocol protocol
#endif /* 2.5.70 => 2.4.x */

/*****************************************************************************/
/* < 2.4.27 or 2.6.0 <= 2.6.5 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,27) || \
    ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) && \
      LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5) ) )

#ifndef netif_msg_init
#define netif_msg_init _kc_netif_msg_init
static inline u32 _kc_netif_msg_init(int debug_value, int default_msg_enable_bits)
{
	/* use default */
	if (debug_value < 0 || debug_value >= (sizeof(u32) * 8))
		return default_msg_enable_bits;
	if (debug_value == 0) /* no output */
		return 0;
	/* set low N bits */
	return (1 << debug_value) -1;
}
#endif

#endif /* < 2.4.27 or 2.6.0 <= 2.6.5 */
/*****************************************************************************/
#if (( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,27) ) || \
     (( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) ) && \
      ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3) )))
#define netdev_priv(x) x->priv
#endif

/*****************************************************************************/
/* <= 2.5.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0) )
#undef pci_register_driver
#define pci_register_driver pci_module_init
#endif /* <= 2.5.0 */

/*****************************************************************************/
/* 2.5.28 => 2.4.23 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,5,28) )

static inline void _kc_synchronize_irq() { synchronize_irq(); }
#undef synchronize_irq
#define synchronize_irq(X) _kc_synchronize_irq()

#include <linux/tqueue.h>
#define work_struct tq_struct
#define INIT_WORK INIT_TQUEUE
#define schedule_work schedule_task
#define flush_scheduled_work flush_scheduled_tasks

#endif /* 2.5.28 => 2.4.17 */

/*****************************************************************************/
/* 2.6.0 => 2.5.28 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) )
#define MODULE_INFO(version, _version)
#ifndef CONFIG_E1000_DISABLE_PACKET_SPLIT
#define CONFIG_E1000_DISABLE_PACKET_SPLIT 1
#endif

#define pci_set_consistent_dma_mask(dev,mask) 1

#undef dev_put
#define dev_put(dev) __dev_put(dev)

#ifndef skb_fill_page_desc
#define skb_fill_page_desc _kc_skb_fill_page_desc
extern void _kc_skb_fill_page_desc(struct sk_buff *skb, int i, struct page *page, int off, int size);
#endif

#ifndef pci_dma_mapping_error
#define pci_dma_mapping_error _kc_pci_dma_mapping_error
static inline int _kc_pci_dma_mapping_error(dma_addr_t dma_addr)
{
	return dma_addr == 0;
}
#endif

#endif /* 2.6.0 => 2.5.28 */

/*****************************************************************************/
/* 2.6.4 => 2.6.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4) )
#define MODULE_VERSION(_version) MODULE_INFO(version, _version)
#endif /* 2.6.4 => 2.6.0 */

/*****************************************************************************/
/* 2.6.5 => 2.6.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5) )
#define pci_dma_sync_single_for_cpu	pci_dma_sync_single
#define pci_dma_sync_single_for_device	pci_dma_sync_single_for_cpu
#endif /* 2.6.5 => 2.6.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7) )
#undef if_mii
#define if_mii _kc_if_mii
static inline struct mii_ioctl_data *_kc_if_mii(struct ifreq *rq)
{
	return (struct mii_ioctl_data *) &rq->ifr_ifru;
}
#endif /* < 2.6.7 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8) )
#define msleep(x)	do { set_current_state(TASK_UNINTERRUPTIBLE); \
				schedule_timeout((x * HZ)/1000 + 2); \
			} while (0)
#endif

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
#define __iomem

#define MSEC_PER_SEC    1000L
static inline unsigned int _kc_jiffies_to_msecs(const unsigned long j)
{
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
	return (MSEC_PER_SEC / HZ) * j;
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
	return (j + (HZ / MSEC_PER_SEC) - 1)/(HZ / MSEC_PER_SEC);
#else
	return (j * MSEC_PER_SEC) / HZ;
#endif
}
static inline unsigned long _kc_msecs_to_jiffies(const unsigned int m)
{
	if (m > _kc_jiffies_to_msecs(MAX_JIFFY_OFFSET))
		return MAX_JIFFY_OFFSET;
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
	return (m + (MSEC_PER_SEC / HZ) - 1) / (MSEC_PER_SEC / HZ);
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
	return m * (HZ / MSEC_PER_SEC);
#else
	return (m * HZ + MSEC_PER_SEC - 1) / MSEC_PER_SEC;
#endif
}

#define msleep_interruptible _kc_msleep_interruptible
static unsigned inline long _kc_msleep_interruptible(unsigned int msecs)
{
	unsigned long timeout = _kc_msecs_to_jiffies(msecs) + 1;

	while (timeout && !signal_pending(current)) {
		__set_current_state(TASK_INTERRUPTIBLE);
		timeout = schedule_timeout(timeout);
	}
	return _kc_jiffies_to_msecs(timeout);
}
#endif /* < 2.6.9 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,6) && \
      LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10) )
#ifdef pci_save_state
#undef pci_save_state
#endif
#define pci_save_state(X) { \
        int i; \
        if (adapter->pci_state) { \
                for (i = 0; i < 16; i++) { \
                        pci_read_config_dword((X), \
                                              i * 4, \
                                              &adapter->pci_state[i]); \
                } \
        } \
}

#ifdef pci_restore_state
#undef pci_restore_state
#endif
#define pci_restore_state(X) { \
        int i; \
        if (adapter->pci_state) { \
                for (i = 0; i < 16; i++) { \
                        pci_write_config_dword((X), \
                                               i * 4, \
                                               adapter->pci_state[i]); \
                } \
        } else { \
                for (i = 0; i < 6; i++) { \
                        pci_write_config_dword((X), \
                                               PCI_BASE_ADDRESS_0 + (i * 4), \
                                               (X)->resource[i].start); \
                } \
        } \
}
#endif /* 2.4.6 <= x < 2.6.10 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10) )
#ifdef module_param_array_named
#undef module_param_array_named
#define module_param_array_named(name, array, type, nump, perm)          \
	static struct kparam_array __param_arr_##name                    \
	= { ARRAY_SIZE(array), nump, param_set_##type, param_get_##type, \
	    sizeof(array[0]), array };                                   \
	module_param_call(name, param_array_set, param_array_get,        \
			  &__param_arr_##name, perm)
#endif /* module_param_array_named */
#endif /* < 2.6.10 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11) )
#define PCI_D0      0
#define PCI_D1      1
#define PCI_D2      2
#define PCI_D3hot   3
#define PCI_D3cold  4
#define pci_choose_state(pdev,state) state
#define PMSG_SUSPEND 3

#undef NETIF_F_LLTX

#ifndef ARCH_HAS_PREFETCH
#define prefetch(X)
#endif

#ifndef NET_IP_ALIGN
#define NET_IP_ALIGN 2
#endif

#endif /* < 2.6.11 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12) )
#define E100_USE_REBOOT_NOTIFIER
#else
#define USE_DRIVER_SHUTDOWN_HANDLER
#define E100_USE_SHUTDOWN_HANDLER
#endif

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14) )
#define pm_message_t u32
#ifndef kzalloc
#define kzalloc _kc_kzalloc
extern void *_kc_kzalloc(size_t size, int flags);
#endif
#endif

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16) )
#undef E100_USE_PCI_ERS
#undef CONFIG_E1000_PCI_ERS
#undef CONFIG_IXGB_PCI_ERS
#else
#define E100_USE_PCI_ERS
#define CONFIG_E1000_PCI_ERS
#define CONFIG_IXGB_PCI_ERS
#endif

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18) )

#ifndef IRQF_PROBE_SHARED
#ifdef SA_PROBEIRQ
#define IRQF_PROBE_SHARED SA_PROBEIRQ
#else
#define IRQF_PROBE_SHARED 0
#endif
#endif

#ifndef IRQF_SHARED
#define IRQF_SHARED SA_SHIRQ
#endif

#ifndef netdev_alloc_skb
#define netdev_alloc_skb _kc_netdev_alloc_skb
extern struct sk_buff *_kc_netdev_alloc_skb(struct net_device *dev,
                                            unsigned int length);
#endif

#ifndef skb_is_gso
#ifdef NETIF_F_TSO
#define skb_is_gso _kc_skb_is_gso
static inline int _kc_skb_is_gso(const struct sk_buff *skb)
{
	return skb_shinfo(skb)->gso_size;
}
#endif
#endif

#endif /* < 2.6.18 */
/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) )

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) )
typedef irqreturn_t (*irq_handler_t)(int, void*, struct pt_regs *);
typedef irqreturn_t (*new_handler_t)(int, void*);
static inline irqreturn_t _kc_request_irq(unsigned int irq, new_handler_t handler, unsigned long flags, const char *devname, void *dev_id)
#else /* 2.4.x */
typedef void (*irq_handler_t)(int, void*, struct pt_regs *);
typedef void (*new_handler_t)(int, void*);
static inline int _kc_request_irq(unsigned int irq, new_handler_t handler, unsigned long flags, const char *devname, void *dev_id)
#endif
{
	irq_handler_t new_handler = (irq_handler_t) handler;
	return request_irq(irq, new_handler, flags, devname, dev_id);
}

#undef request_irq
#define request_irq(irq, handler, flags, devname, dev_id) _kc_request_irq((irq), (handler), (flags), (devname), (dev_id))

#endif /* < 2.6.19 */
/*****************************************************************************/

#endif /* _KCOMPAT_H_ */

