/*******************************************************************************

  
  Copyright(c) 1999 - 2002 Intel Corporation. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information:
  Linux NICS <linux.nics@intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _KCOMPAT_H_
#define _KCOMPAT_H_

#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#define __NO_VERSION__
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <asm/io.h>

/*****************************************************************************/
/* 2.4.0 => 2.2.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) )

/**************************************/
/* MODULE API */

#ifndef __init
	#define __init
#endif

#ifndef __exit
	#define __exit
#endif

#ifndef __devinit
	#define __devinit
#endif

#ifndef __devexit
	#define __devexit
#endif

#ifndef __devinitdata
	#define __devinitdata
#endif

#ifndef module_init
	#define module_init(f) int init_module() { return f(); }
#endif

#ifndef module_exit
	#define module_exit(f) void cleanup_module() { return f(); }
#endif

#ifndef MODULE_DEVICE_TABLE
	#define MODULE_DEVICE_TABLE(bus, table)
#endif

#ifndef SET_MODULE_OWNER
	#define SET_MODULE_OWNER(X)
#else
	#undef MOD_INC_USE_COUNT
	#undef MOD_DEC_USE_COUNT
	#define MOD_INC_USE_COUNT
	#define MOD_DEC_USE_COUNT
#endif

/**************************************/
/* PCI DRIVER API */

#ifndef pci_device_id
#define pci_device_id _kc_pci_device_id
struct _kc_pci_device_id {
	unsigned int vendor, device;
	unsigned int subvendor, subdevice;
	unsigned int class, classmask;
	unsigned long driver_data;
};
#endif

#ifndef pci_driver
#define pci_driver _kc_pci_driver
struct _kc_pci_driver {
	char *name;
	struct pci_device_id *id_table;
	int (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
	void (*remove)(struct pci_dev *dev);
	int (*save_state)(struct pci_dev *dev, u32 state);
	int (*suspend)(struct pci_dev *dev, u32 state);
	int (*resume)(struct pci_dev *dev);
	int (*enable_wake)(struct pci_dev *dev, u32 state, int enable);
};
#endif

#ifndef PCI_ANY_ID
	#define PCI_ANY_ID (~0U)
#endif

#ifndef pci_module_init
#define pci_module_init _kc_pci_module_init
extern int _kc_pci_module_init(struct pci_driver *drv);
#endif

#ifndef pci_unregister_driver
#define pci_unregister_driver _kc_pci_unregister_driver
extern void _kc_pci_unregister_driver(struct pci_driver *drv);
#endif

#ifndef pci_set_drvdata
#define pci_set_drvdata _kc_pci_set_drvdata
extern void _kc_pci_set_drvdata(struct pci_dev *dev, void *data);
#endif

#ifndef pci_get_drvdata
#define pci_get_drvdata _kc_pci_get_drvdata
extern void * _kc_pci_get_drvdata(struct pci_dev *dev);
#endif

#ifndef pci_enable_device
#define pci_enable_device _kc_pci_enable_device
extern int _kc_pci_enable_device(struct pci_dev *dev);
#endif

#ifndef pci_resource_start
#define pci_resource_start _kc_pci_resource_start
extern int _kc_pci_resource_start(struct pci_dev *dev, int bar);
#endif

#ifndef pci_resource_len
#define pci_resource_len _kc_pci_resource_len
extern unsigned long _kc_pci_resource_len(struct pci_dev *dev, int bar);
#endif

#ifndef pci_for_each_dev
#define pci_for_each_dev(dev) for(dev = pci_devices; dev; dev = dev->next)
#endif

#ifndef pci_dev_driver
#define pci_dev_driver _kc_pci_dev_driver
extern struct pci_driver *_kc_pci_dev_driver(struct pci_dev *dev);
#endif

#undef IORESOURCE_IO
#define IORESOURCE_IO PCI_BASE_ADDRESS_SPACE_IO

#undef pci_resource_flags
#define pci_resource_flags(dev, i) (dev->base_address[i] & IORESOURCE_IO)

/**************************************/
/* PCI DMA MAPPING */

#ifndef PCI_DMA_TODEVICE
	#define PCI_DMA_TODEVICE 1
#endif

#ifndef PCI_DMA_FROMDEVICE
	#define PCI_DMA_FROMDEVICE 2
#endif

#ifndef dma_addr_t
#define dma_addr_t _kc_dma_addr_t
typedef u64 _kc_dma_addr_t;
#endif

#ifndef pci_alloc_consistent
#define pci_alloc_consistent _kc_pci_alloc_consistent
extern void * _kc_pci_alloc_consistent(struct pci_dev *dev, size_t size, u64 *dma_handle);
#endif

#ifndef pci_free_consistent
#define pci_free_consistent _kc_pci_free_consistent
extern void _kc_pci_free_consistent(struct pci_dev *dev, size_t size, void *vaddr, u64 dma_handle);
#endif

#ifndef pci_map_single
#define pci_map_single _kc_pci_map_single
extern u64 _kc_pci_map_single(struct pci_dev *dev, void *addr, size_t size, int direction);
#endif

#ifndef pci_unmap_single
#define pci_unmap_single _kc_pci_unmap_single
extern void _kc_pci_unmap_single(struct pci_dev *dev, u64 dma_addr, size_t size, int direction);
#endif

#ifndef pci_dma_sync_single
#define pci_dma_sync_single _kc_pci_dma_sync_single
extern void _kc_pci_dma_sync_single(struct pci_dev *dev, u64 dma_addr, size_t size, int direction);
#endif

/**************************************/
/* NETWORK DRIVER API */

#ifndef net_device
	#define net_device device
#endif

#ifndef dev_kfree_skb_irq
	#define dev_kfree_skb_irq dev_kfree_skb
#endif

#ifndef dev_kfree_skb_any
	#define dev_kfree_skb_any dev_kfree_skb
#endif

#ifndef netif_running
	#define netif_running(dev) (!!(int)(dev->flags & IFF_RUNNING))
#endif

#ifndef netif_start_queue
	#define netif_start_queue(dev) clear_bit(0, &dev->tbusy)
#endif

#ifndef netif_stop_queue
	#define netif_stop_queue(dev) set_bit(0, &dev->tbusy)
#endif

#ifndef netif_wake_queue
	#define netif_wake_queue(dev) do { clear_bit(0, &dev->tbusy); \
		                           mark_bh(NET_BH); } while(0)
#endif

#ifndef netif_queue_stopped
	#define netif_queue_stopped(dev) (dev->tbusy)
#endif

#ifndef netif_device_attach
#define netif_device_attach _kc_netif_device_attach
extern void _kc_netif_device_attach(struct net_device *dev);
#endif

#ifndef netif_device_detach
#define netif_device_detach _kc_netif_device_detach
extern void _kc_netif_device_detach(struct net_device *dev);
#endif

#ifndef netif_carrier_on
#define netif_carrier_on _kc_netif_carrier_on
extern void _kc_netif_carrier_on(struct net_device *dev);
#endif

#ifndef netif_carrier_off
#define netif_carrier_off _kc_netif_carrier_off
extern void _kc_netif_carrier_off(struct net_device *dev);
#endif

#ifndef netif_carrier_ok
#define netif_carrier_ok _kc_netif_carrier_ok
extern int _kc_netif_carrier_ok(struct net_device *dev);
#endif

/**************************************/
/* OTHER */

#ifndef del_timer_sync
	#define del_timer_sync del_timer
#endif

#ifndef BUG
	#define BUG() printk(KERN_CRIT "BUG in %s at line %d\n", __FILE__, __LINE__)
#endif

#ifndef set_current_state
	#define set_current_state(S) current->state = (S)
#endif

#ifndef list_for_each
	#define list_for_each(pos, head) \
		for(pos = (head)->next; pos != (head); pos = pos->next)
#endif

#ifndef list_add_tail
	#define list_add_tail(new, head) __list_add(new, (head)->prev, (head))
#endif

#ifndef ARRARY_SIZE
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef init_MUTEX
	#define init_MUTEX(x) do{*(x) = MUTEX;}while(0)
#endif

#define msec_delay(x) mdelay(x)

#else /* remove 2.2.x only compatibility stuff */

	#undef MOD_INC_USE_COUNT
	#undef MOD_DEC_USE_COUNT
	#define MOD_INC_USE_COUNT
	#define MOD_DEC_USE_COUNT

#endif /* 2.4.0 => 2.2.0 */


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

/* PCI PM entry point syntax changed, so don't support suspend/resume */
#undef CONFIG_PM

#endif /* 2.4.6 => 2.4.3 */

/*****************************************************************************/
/* 2.4.10 => 2.4.6 */
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

#undef PCI_DMA_32BIT
#define PCI_DMA_32BIT	0xffffffff
#undef PCI_DMA_64BIT
#define PCI_DMA_64BIT	0xffffffff

#endif /* 2.4.13 => 2.4.10 */

/*****************************************************************************/
/* 2.4.17 => 2.4.12 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,17) )

#ifndef __devexit_p
	#define __devexit_p(x) &(x)
#endif

#endif /* 2.4.17 => 2.4.13 */

#endif /* _KCOMPAT_H_ */

