/*****************************************************************************
 *****************************************************************************
 Copyright (c) 1999-2000, Intel Corporation 

 All rights reserved.

 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, 
 this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation 
 and/or other materials provided with the distribution.

 3. Neither the name of Intel Corporation nor the names of its contributors 
 may be used to endorse or promote products derived from this software 
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

 *****************************************************************************
****************************************************************************/

/* Macros to make drivers compatible with 2.2.0 - 2.3.51 Linux kernels
 *
 * In order to make a single network driver work with all 2.2-2.4 kernels
 * these compatibility macros can be used.
 * They are backwards compatible implementations of the latest APIs.
 * The idea is that these macros will let you use the newest driver with old
 * kernels, but can be removed when working with the latest and greatest.
 */

#ifndef LINUX_KERNEL_COMPAT_H
#define LINUX_KERNEL_COMPAT_H

#include <linux/version.h>

/*****************************************************************************
 **
 **  PCI Bus Changes
 **
 *****************************************************************************/

/* Accessing the BAR registers from the PCI device structure
 * Changed from base_address[bar] to resource[bar].start in 2.3.13
 * The pci_resource_start inline function was introduced in 2.3.43 
 */
#if   ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,13) )
#define pci_resource_start(dev, bar) (dev)->base_address[(bar)]
#elif ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43) )
#define pci_resource_start(dev, bar) (dev)->resource[(bar)].start
#endif

/* Starting with 2.3.23 drivers are supposed to call pci_enable_device
 * to make sure I/O and memory regions have been mapped and potentially 
 * bring the device out of a low power state
 */
#if   ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,23) )
#define pci_enable_device(dev) do{} while(0)
#endif

/* Dynamic DMA mapping
 * Instead of using virt_to_bus, bus mastering PCI drivers should use the DMA 
 * mapping API to get bus addresses.  This lets some platforms use dynamic 
 * mapping to use PCI devices that do not support DAC in a 64-bit address space
 */
#if   ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,41) )
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <asm/io.h>

#if   ( LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18) )
typedef unsigned long dma_addr_t;
#endif

extern inline void *pci_alloc_consistent (struct pci_dev *dev, 
                                          size_t size, 
                                          dma_addr_t *dma_handle) {
    void *vaddr = kmalloc(size, GFP_KERNEL);
    if(vaddr != NULL) {
        memset(vaddr, 0, size);
        *dma_handle = virt_to_bus(vaddr);
    }
    return vaddr; 
}

#define pci_dma_supported(dev, addr_mask)                    (1)
#define pci_free_consistent(dev, size, cpu_addr, dma_handle) kfree(cpu_addr)
#define pci_map_single(dev, addr, size, direction)           virt_to_bus(addr)
#define pci_unmap_single(dev, dma_handle, size, direction)   do{} while(0)
#endif

/*****************************************************************************
 **
 **  Network Device API Changes
 **
 *****************************************************************************/

/* In 2.3.14 the device structure was renamed to net_device 
 */
#if   ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,14) )
#define net_device device
#endif

/* 'Softnet' network stack changes merged in 2.3.43 
 * these are 2.2 compatible defines for the new network interface API
 * 2.3.47 added some more inline functions for softnet to remove explicit 
 * bit tests in drivers
 */
#if   ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43) )
#define netif_start_queue(dev)   clear_bit  (0, &(dev)->tbusy)
#define netif_stop_queue(dev)    set_bit    (0, &(dev)->tbusy)
#define netif_wake_queue(dev)    { clear_bit(0, &(dev)->tbusy); \
                                                mark_bh(NET_BH); }
#define netif_running(dev)       test_bit(0, &(dev)->start)
#define netif_queue_stopped(dev) test_bit(0, &(dev)->tbusy)
#elif ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,47) )
#define netif_running(dev)       test_bit(LINK_STATE_START, &(dev)->state)
#define netif_queue_stopped(dev) test_bit(LINK_STATE_XOFF,  &(dev)->state)
#endif

/* Softnet changes also affected how SKBs are handled
 * Special calls need to be made now while in an interrupt handler
 */
#if   ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43) )
#define dev_kfree_skb_irq(skb) dev_kfree_skb(skb)
#endif

/*****************************************************************************
 **
 **  General Module / Driver / Kernel API Changes
 **
 *****************************************************************************/

/* New module_init macro added in 2.3.13 - replaces init_module entry point
 * If MODULE is defined, it expands to an init_module definition
 * If the driver is staticly linked to the kernel, it creates the proper 
 * function	pointer for the initialization routine to be called
 * (no more Space.c)
 * module_exit does the same thing for cleanup_module
 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,13) )
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18) ) || ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0) )
#define module_init(fn) int  init_module   (void) { return fn(); }
#define module_exit(fn) void cleanup_module(void) { return fn(); }
#endif
#endif

#endif /* LINUX_KERNEL_COMPAT_H */

