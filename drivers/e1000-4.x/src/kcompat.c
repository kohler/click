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

#include "kcompat.h"

struct _kc_pci_dev_ext {
	struct pci_dev *dev;
	void *pci_drvdata;
	struct pci_driver *driver;
};

struct _kc_net_dev_ext {
	struct net_device *dev;
	unsigned int carrier;
};


/*****************************************************************************/
/* 2.4.13 => 2.4.3 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,13) )

/**************************************/
/* PCI DMA MAPPING */

#if defined(CONFIG_HIGHMEM)

#ifndef PCI_DRAM_OFFSET
#define PCI_DRAM_OFFSET 0
#endif

u64 _kc_pci_map_page(struct pci_dev *dev, struct page *page, unsigned long offset, size_t size, int direction)
{
	return (((u64)(page - mem_map) << PAGE_SHIFT) + offset + PCI_DRAM_OFFSET);
}

#else /* CONFIG_HIGHMEM */

u64 _kc_pci_map_page(struct pci_dev *dev, struct page *page, unsigned long offset, size_t size, int direction)
{
	return pci_map_single(dev, (void *)page_address(page) + offset, size, direction);
}

#endif /* CONFIG_HIGHMEM */

void _kc_pci_unmap_page(struct pci_dev *dev, u64 dma_addr, size_t size, int direction)
{
	return pci_unmap_single(dev, dma_addr, size, direction);
}

#endif /* 2.4.13 => 2.4.3 */


/*****************************************************************************/
/* 2.4.3 => 2.4.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3) )

/**************************************/
/* PCI DRIVER API */

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) )
int _kc_pci_set_dma_mask(struct pci_dev *dev, dma_addr_t mask) { return 0; }
#else
int _kc_pci_set_dma_mask(struct pci_dev *dev, dma_addr_t mask)
{
	if(!pci_dma_supported(dev, mask))
		return -EIO;
	dev->dma_mask = mask;
	return 0;
}
#endif

int _kc_pci_request_regions(struct pci_dev *dev, char *res_name)
{
	int i;

	for (i = 0; i < 6; i++) {
		if (pci_resource_len(dev, i) == 0)
			continue;

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) )
		if ((dev->base_address[i] & PCI_BASE_ADDRESS_SPACE_IO))
			request_region(pci_resource_start(dev, i), pci_resource_len(dev, i), res_name);
#else
		if (pci_resource_flags(dev, i) & IORESOURCE_IO) {
			if (!request_region(pci_resource_start(dev, i), pci_resource_len(dev, i), res_name)) {
				pci_release_regions(dev);
				return -EBUSY;
			}
		} else if (pci_resource_flags(dev, i) & IORESOURCE_MEM) {
			if (!request_mem_region(pci_resource_start(dev, i), pci_resource_len(dev, i), res_name)) {
				pci_release_regions(dev);
				return -EBUSY;
			}
		}
#endif
	}
	return 0;
}

void _kc_pci_release_regions(struct pci_dev *dev)
{
	int i;

	for (i = 0; i < 6; i++) {
		if (pci_resource_len(dev, i) == 0)
			continue;

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) )
		if ((dev->base_address[i] & PCI_BASE_ADDRESS_SPACE))
			release_region(pci_resource_start(dev, i), pci_resource_len(dev, i));
#else
		if (pci_resource_flags(dev, i) & IORESOURCE_IO)
			release_region(pci_resource_start(dev, i), pci_resource_len(dev, i));

		else if (pci_resource_flags(dev, i) & IORESOURCE_MEM)
			release_mem_region(pci_resource_start(dev, i), pci_resource_len(dev, i));
#endif
	}
}

/**************************************/
/* NETWORK DRIVER API */

#define _KC_MAX_NET_DEV 32
static int my_net_count = 0;
static struct _kc_net_dev_ext my_net_devices[_KC_MAX_NET_DEV];

struct net_device * _kc_alloc_etherdev(int sizeof_priv)
{
	struct net_device *dev;
	int alloc_size;

	if(my_net_count >= _KC_MAX_NET_DEV)
		return NULL;

	alloc_size = sizeof (*dev) + sizeof_priv + IFNAMSIZ + 31;

	dev = kmalloc(alloc_size, GFP_KERNEL);

	if (!dev) return NULL;

	memset(dev, 0, alloc_size);

	if (sizeof_priv)
		dev->priv = (void *) (((unsigned long)(dev + 1) + 31) & ~31);

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) )
	dev->name = (char *) dev->priv + sizeof_priv;
#endif
	dev->name[0] = '\0';

	ether_setup(dev);

	my_net_devices[my_net_count].dev = dev;
	my_net_count++;

	return dev;
}

int _kc_is_valid_ether_addr(u8 *addr)
{
	const char zaddr[6] = {0,};

	return !(addr[0]&1) && memcmp( addr, zaddr, 6);
}

#endif /* 2.4.3 => 2.4.0 */


/*****************************************************************************/
/* 2.4.0 => 2.2.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) )

/**************************************/
/* PCI DRIVER API */

#define _KC_MAX_PCI_DEV 32
static int my_pci_count = 0;
static struct _kc_pci_dev_ext my_pci_devices[_KC_MAX_PCI_DEV];

int _kc_pci_module_init(struct pci_driver *drv)
{
	struct pci_dev *dev;
	struct pci_device_id *pciid;
	uint16_t subvendor, subdevice;

	my_pci_count = 0;

	for(dev = pci_devices; dev; dev = dev->next) {

		if(my_pci_count >= _KC_MAX_PCI_DEV)
			break;

		pciid = &drv->id_table[0];
		pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID, &subvendor);
		pci_read_config_word(dev, PCI_SUBSYSTEM_ID, &subdevice);

		while(pciid->vendor != 0) {
			if(((pciid->vendor == dev->vendor) ||
			   (pciid->vendor == PCI_ANY_ID)) &&

			  ((pciid->device == dev->device) ||
			   (pciid->device == PCI_ANY_ID)) &&

			  ((pciid->subvendor == subvendor) ||
			   (pciid->subvendor == PCI_ANY_ID)) &&

			  ((pciid->subdevice == subdevice) ||
			   (pciid->subdevice == PCI_ANY_ID))) {

				my_pci_devices[my_pci_count].dev = dev;
				my_pci_devices[my_pci_count].driver = drv;
				my_pci_count++;
				if(drv->probe(dev, pciid)) {
					my_pci_count--;
					my_pci_devices[my_pci_count].dev = NULL;
				}
				break;
			}
			pciid++;
		}
	}
	return (my_pci_count > 0) ? 0 : -ENODEV;
}

void _kc_pci_unregister_driver(struct pci_driver *drv)
{
	int i;
	for(i = 0; i < my_pci_count; i++) {
		if(my_pci_devices[i].dev) {
			drv->remove(my_pci_devices[i].dev);
			my_pci_devices[i].dev = NULL;
		}
	}
	my_pci_count = 0;
}

void _kc_pci_set_drvdata(struct pci_dev *dev, void *data)
{
	int i;
	for(i = 0; i < my_pci_count; i++) {
		if(my_pci_devices[i].dev == dev) {
			my_pci_devices[i].pci_drvdata = data;
		}
	}
}

void * _kc_pci_get_drvdata(struct pci_dev *dev)
{
	int i;
	for(i = 0; i < my_pci_count; i++) {
		if(my_pci_devices[i].dev == dev) {
			return my_pci_devices[i].pci_drvdata;
		}
	}
	return NULL;
}

int _kc_pci_enable_device(struct pci_dev *dev) { return 0; }

int _kc_pci_resource_start(struct pci_dev *dev, int bar)
{
	return ((dev->base_address[bar] & PCI_BASE_ADDRESS_SPACE) ?
		(dev->base_address[bar] & PCI_BASE_ADDRESS_IO_MASK) :
		(dev->base_address[bar] & PCI_BASE_ADDRESS_MEM_MASK));
}

unsigned long _kc_pci_resource_len(struct pci_dev *dev, int bar)
{
	u32 old, len;

	int bar_reg = PCI_BASE_ADDRESS_0 + (bar << 2);

	pci_read_config_dword(dev, bar_reg, &old);
	pci_write_config_dword(dev, bar_reg, ~0);
	pci_read_config_dword(dev, bar_reg, &len);
	pci_write_config_dword(dev, bar_reg, old);

	if((len & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY)
		len = ~(len & PCI_BASE_ADDRESS_MEM_MASK);
	else
		len = ~(len & PCI_BASE_ADDRESS_IO_MASK) & 0xffff;
	return (len + 1);
}

struct pci_driver *_kc_pci_dev_driver(struct pci_dev *dev)
{
	int i;
	for(i = 0; i < my_pci_count; i++) {
		if(my_pci_devices[i].dev == dev) {
			return my_pci_devices[i].driver;
		}
	}
	return NULL;
}

/**************************************/
/* PCI DMA MAPPING */

void * _kc_pci_alloc_consistent(struct pci_dev *dev, size_t size, u64 *dma_handle)
{
	void *vaddr = kmalloc(size, GFP_KERNEL);

	if(vaddr)
		*dma_handle = virt_to_bus(vaddr);
	return vaddr;
}

void _kc_pci_free_consistent(struct pci_dev *dev, size_t size, void *addr, u64 dma_handle)
{
	kfree(addr);
}

u64 _kc_pci_map_single(struct pci_dev *dev, void *addr, size_t size, int direction)
{
	return virt_to_bus(addr);
}

void _kc_pci_unmap_single(struct pci_dev *dev, u64 dma_addr, size_t size, int direction) { return; }

void _kc_pci_dma_sync_single(struct pci_dev *dev, u64 dma_addr, size_t size, int direction) { return; }

/**************************************/
/* NETWORK DRIVER API */

void _kc_netif_device_attach(struct net_device *dev)
{
	if(netif_running(dev) && netif_queue_stopped(dev)) {
		netif_wake_queue(dev);
	}
}

void _kc_netif_device_detach(struct net_device *dev)
{
	if(netif_running(dev)) {
		netif_stop_queue(dev);
	}
}

void _kc_netif_carrier_on(struct net_device *dev)
{
	int i;
	for(i = 0; i < my_net_count; i++) {
		if(my_net_devices[i].dev == dev) {
			set_bit(0, &my_net_devices[i].carrier);
		}
	}
}

void _kc_netif_carrier_off(struct net_device *dev)
{
	int i;
	for(i = 0; i < my_net_count; i++) {
		if(my_net_devices[i].dev == dev) {
			clear_bit(0, &my_net_devices[i].carrier);
		}
	}
}

int _kc_netif_carrier_ok(struct net_device *dev)
{
	int i;
	for(i = 0; i < my_net_count; i++) {
		if(my_net_devices[i].dev == dev) {
			return test_bit(0, &my_net_devices[i].carrier);
		}
	}
	return 0;
}

#endif /* 2.4.0 => 2.2.0 */

/*****************************************************************************/
/* 2.4.6 => 2.4.3 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,6) )

int _kc_pci_set_power_state(struct pci_dev *dev, int state)
{ return 0; }
int _kc_pci_save_state(struct pci_dev *dev, u32 *buffer)
{ return 0; }
int _kc_pci_restore_state(struct pci_dev *pdev, u32 *buffer)
{ return 0; }
int _kc_pci_enable_wake(struct pci_dev *pdev, u32 state, int enable)
{ return 0; }

#endif /* 2.4.6 => 2.4.3 */

