/*****************************************************************************
 *****************************************************************************
Copyright (c) 1999 - 2000, Intel Corporation 

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

/**********************************************************************
*                                                                     *
* INTEL CORPORATION                                                   *
*                                                                     *
* This software is supplied under the terms of the license included   *
* above.  All use of this driver must be in accordance with the terms *
* of that license.                                                    *
*                                                                     *
* Module Name:  e1000.c                                               *
*                                                                     *
* Abstract:     Functions for the driver entry points like load,      *
*               unload, open and close. All board specific calls made *
*               by the network interface section of the driver.       *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/

 
/* This first section contains the setable parametes for configuring the 
 * driver into the kernel.
 */

#define E1000_DEBUG_DEFAULT 0
static int e1000_debug_level = E1000_DEBUG_DEFAULT;

/* global defines */
#define MAX_TCB      256            /* number of transmit descriptors */
#define MAX_RFD      256            /* number of receive descriptors */

/* includes */
#ifdef MODULE
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#endif
#define E1000_MAIN_STATIC
#ifdef IANS
#define _IANS_MAIN_MODULE_C_
#endif

#include "e1000.h"
#include "e1000_vendor_info.h"
#include "e1000_proc.h"

/* Global Data structures and variables */
static const char *version =
"Intel(R) PRO/1000 Gigabit Ethernet Adapter - Loadable driver, ver. 2.5.11\n";
static char e1000_copyright[] = "         Copyright (c) 1999-2000 Intel Corporation\n";
static char e1000id_string[128] = "Intel(R) PRO/1000 Gigabit Server Adapter";
static char e1000_driver[] = "e1000";
static char e1000_version[] = "2.5.11";

static bd_config_t *e1000first = NULL;
static int e1000boards = 0;
static int e1000_rxfree_cnt = 0;

/* Driver performance tuning variables */
static uint_t e1000_pcimlt_override = 0;
static uint_t e1000_pcimwi_enable = 1;
static uint_t e1000_txint_delay = 128;
#if 1
static uint_t e1000_rxint_delay = 128; /* RTM turn on rcv intr coalescing? microseconds? */
#else
static uint_t e1000_rxint_delay = 0;
#endif

#if 1
static int e1000_flow_ctrl = 0; /* RTM turn off flow control */
#else
static int e1000_flow_ctrl = 3;
#endif
static int e1000_rxpci_priority = 0;
static uint_t e1000_maxdpc_count = 5;

static int TxDescriptors[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int RxDescriptors[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

/* Feature enable/disable */
static int Jumbo[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int WaitForLink[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int SBP[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* Speed and Duplex Settings */
static int AutoNeg[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int Speed[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int ForceDuplex[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

/* This parameter determines whether the driver sets the RS bit or the
 * RPS bit in a transmit packet. These are the possible values for this
 * parameter:
 * e1000_ReportTxEarly = 0  ==>  set the RPS bit
 *                      = 1  ==>  set the RS bit
 *                      = 2  ==>  (default) let the driver auto-detect
 *
 * If the RS bit is set in a packet, an interrupts is generated as soon
 * as the packet is DMAed from host memory to the FIFO. If the RPS bit
 * is set, an interrupt is generated after the packet has been transmitted
 * on the wire.
 */
static uchar_t e1000_ReportTxEarly = 2;    /* let the driver auto-detect */


/* these next ones are for Linux specific things */
static int probed = 0;

static int e1000_tx_queue(struct device *dev, struct sk_buff *skb);
static int e1000_tx_start(struct device *dev);
static int e1000_rx_refill(struct device *dev, struct sk_buff **);
static int e1000_tx_eob(struct device *dev);
static struct sk_buff *e1000_tx_clean(struct device *dev);
static struct sk_buff *e1000_rx_poll(struct device *dev, int *want);
static int e1000_poll_on(struct device *dev);
static int e1000_poll_off(struct device *dev);

/* The system interface routines */

/****************************************************************************
* Name:        e1000_probe
*
* Description: This routine is called when the dynamic driver module
*              "e1000" is loaded using the command "insmod".
*
*               This is a Linux required routine.
*
* Author:      IntelCorporation
*
* Born on Date:    07/11/99
*
* Arguments:   
*       NONE
*
* Returns:
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
int
e1000_probe()
{
    device_t *dev;
    bd_config_t *bdp;
    PADAPTER_STRUCT Adapter;
    pci_dev_t *pcid = NULL;
    int num_boards = 0;
    int first_time = 0, loop_cnt = 0;

    if (e1000_debug_level >= 1)
        printk("e1000_probe()\n");

    /* Has the probe routine been called before? The driver can be probed only
     * once.
     */
    if (probed)
        return (0);
    else
        probed++;

    /* does the system support pci? */
    if ((!CONFIG_PCI) || (!pci_present()))
        return (0);

#ifdef CONFIG_PROC_FS
  {
    int     len;

    /* first check if e1000_proc_dir already exists */
    len = strlen(ADAPTERS_PROC_DIR);
    for (e1000_proc_dir=proc_net->subdir;e1000_proc_dir;
        e1000_proc_dir=e1000_proc_dir->next) {
        if ((e1000_proc_dir->namelen == len) &&
            (!memcmp(e1000_proc_dir->name, ADAPTERS_PROC_DIR, len)))
            break;
        }
        if (!e1000_proc_dir)
            e1000_proc_dir = 
                create_proc_entry(ADAPTERS_PROC_DIR, S_IFDIR, proc_net);
        if (!e1000_proc_dir) return -ENODEV;
  }
#endif
    
    /* loop through all of the ethernet PCI devices looking for ours */
    while ((pcid = pci_find_class(PCI_CLASS_NETWORK_ETHERNET << 8, pcid))) {
        dev = NULL;

        if (e1000_debug_level >= 2)
            printk("e1000_probe: vendor = 0x%x, device = 0x%x \n", 
                    pcid->vendor, pcid->device);
                 

        /* is the device ours? */
        if ((pcid->vendor != E1000_VENDOR_ID) ||
            !((pcid->device == WISEMAN_DEVICE_ID) ||
              (pcid->device == LIVENGOOD_FIBER_DEVICE_ID) ||
              (pcid->device == LIVENGOOD_COPPER_DEVICE_ID))) continue;    /* No, continue */

        if (!first_time) {
            /* only display the version message one time */
            first_time = 1;

            /* print out the version */
            printk("%s", version);
            printk("%s\n", e1000_copyright);
        }

        /* register the net device, but don't allocate a private structure yet */
        dev = init_etherdev(dev, 0);

        if (dev == NULL) {
            printk(KERN_ERR "Not able to alloc etherdev struct\n");
            break;
        }

        /* Allocate all the memory that the driver will need */
        if (!(bdp = e1000_alloc_space())) {
            printk("%s - Failed to allocate memory\n", e1000_driver);
            e1000_dealloc_space(bdp);
            return 0;
        }
        bdp->device = dev;

#ifdef CONFIG_PROC_FS
        if (e1000_create_proc_dev(bdp) < 0) {
            e1000_remove_proc_dev(dev);
            e1000_dealloc_space(bdp);
            continue; /*return e100nics;*/
        }
#endif        

        /* init the timer */
        bdp->timer_val = -1;

        /* point the Adapter to the bddp */
        Adapter = (PADAPTER_STRUCT) bdp->bddp;

#ifdef IANS
        bdp->iANSdata = kmalloc(sizeof(iANSsupport_t), GFP_KERNEL);
        memset((PVOID) bdp->iANSdata, 0, sizeof(iANSsupport_t));
        bd_ans_drv_InitANS(bdp, bdp->iANSdata);
#endif

        /*
         * Obtain the PCI specific information about the driver.
         */
        if (e1000_find_pci_device(pcid, Adapter) == 0) {
            printk("%s - Failed to find PCI device\n", e1000_driver);
            return (0);
        }

          /* range check the command line parameters for this board */
        if(!e1000_GetBrandingMesg(Adapter->DeviceId, Adapter->SubVendorId, Adapter->SubSystemId)) {
            continue;    
        }    
        printk("%s\n", e1000id_string);
         e1000_check_options(loop_cnt);

    switch (Speed[loop_cnt]) {
    case SPEED_10:
        switch(ForceDuplex[loop_cnt]) {
        case HALF_DUPLEX:
            Adapter->AutoNeg = 0;
            Adapter->ForcedSpeedDuplex = HALF_10;
            break;
        case FULL_DUPLEX:
            Adapter->AutoNeg = 0;
            Adapter->ForcedSpeedDuplex = FULL_10;
            break;
        default:
            Adapter->AutoNeg = 1;
            Adapter->AutoNegAdvertised =
                ADVERTISE_10_HALF | ADVERTISE_10_FULL;
        }
        break;
    case SPEED_100:
        switch(ForceDuplex[loop_cnt]) {
        case HALF_DUPLEX:
            Adapter->AutoNeg = 0;
            Adapter->ForcedSpeedDuplex = HALF_100;
            break;
        case FULL_DUPLEX:
            Adapter->AutoNeg = 0;
            Adapter->ForcedSpeedDuplex = FULL_100;
            break;
        default:
            Adapter->AutoNeg = 1;
            Adapter->AutoNegAdvertised =
                ADVERTISE_100_HALF | ADVERTISE_100_FULL;
        }
        break;
    case SPEED_1000:
        Adapter->AutoNeg = 1;
        Adapter->AutoNegAdvertised = ADVERTISE_1000_FULL;
        break;
    default:
        Adapter->AutoNeg = 1;
        switch(ForceDuplex[loop_cnt]) {
        case HALF_DUPLEX:
            Adapter->AutoNegAdvertised = 
                ADVERTISE_10_HALF | ADVERTISE_100_HALF; 
            break;
        case FULL_DUPLEX:
            Adapter->AutoNegAdvertised = 
                ADVERTISE_10_FULL | ADVERTISE_100_FULL | ADVERTISE_1000_FULL; 
            break;
        default:
            Adapter->AutoNegAdvertised = AutoNeg[loop_cnt];
        }
    }


        Adapter->WaitAutoNegComplete = WaitForLink[loop_cnt];


        /* Set Adapter->MacType */
        switch (pcid->device) {
        case WISEMAN_DEVICE_ID:
            if (Adapter->RevID == WISEMAN_2_0_REV_ID)
                Adapter->MacType = MAC_WISEMAN_2_0;
            else {
                if (Adapter->RevID == WISEMAN_2_1_REV_ID)
                    Adapter->MacType = MAC_WISEMAN_2_1;
                else {
                    Adapter->MacType = MAC_WISEMAN_2_0;
                    printk(KERN_ERR
                           "Could not identify hardware revision\n");
                }
            }
            break;
        case LIVENGOOD_FIBER_DEVICE_ID:
        case LIVENGOOD_COPPER_DEVICE_ID:
            Adapter->MacType = MAC_LIVENGOOD;
            break;
        default:
            Adapter->MacType = MAC_WISEMAN_2_0;
            printk(KERN_ERR "Could not identify hardware revision\n");
            break;
        }

        /* save off the needed information */
        bdp->device = dev;
        dev->priv = bdp;
        Adapter = bdp->bddp;

        /* save off the pci device structure pointer */
        Adapter->pci_dev = pcid;
        bdp->vendor = pcid->vendor;

        /* set the irq into the dev and bdp structures */
        dev->irq = pcid->irq;
        bdp->irq_level = pcid->irq;


        /* point to all of our entry point to let the system know where we are */
        dev->open = &e1000_open;
        dev->hard_start_xmit = &e1000_xmit_frame;
        dev->stop = &e1000_close;
        dev->get_stats = &e1000_get_stats;
        dev->set_multicast_list = &e1000_set_multi;
        dev->set_mac_address = &e1000_set_mac;
        dev->change_mtu = &e1000_change_mtu;
#ifdef IANS
        dev->do_ioctl = &bd_ans_os_Ioctl;
#endif
        /* set memory base addr */
        dev->base_addr = pci_resource_start(pcid, 0);

#ifdef CLICK_POLLING
	/* Click - polling extensions */
	dev->polling = 0;
	dev->rx_poll = e1000_rx_poll;
	dev->rx_refill = e1000_rx_refill;
	dev->tx_eob = e1000_tx_eob;
	dev->tx_clean = e1000_tx_clean;
	dev->tx_queue = e1000_tx_queue;
	dev->tx_start = e1000_tx_start;
	dev->poll_off = e1000_poll_off;
	dev->poll_on = e1000_poll_on;
#endif

        /* now map the phys addr into system space... */
        /*
         * Map the PCI memory base address to a virtual address that can be used
         * for access by the driver. The amount of memory to be mapped will be 
         * the E1000 register area.
         */

        Adapter->HardwareVirtualAddress =
            (PE1000_REGISTERS) ioremap(pci_resource_start(pcid, 0),
                                       sizeof(E1000_REGISTERS));

        if (Adapter->HardwareVirtualAddress == NULL) {
            /* 
             * If the hardware register space can not be allocated, the driver
             * must fail to load.
             */
            printk("e1000_probe ioremap failed\n");

            /* this is a problem, if the first one inits OK but a secondary one
             * fails,  what should you return?  Now it will say the load was OK
             * but one or more boards may have failed to come up
             */

            break;
        }

        bdp->mem_start = pci_resource_start(pcid, 0);

        if (e1000_debug_level >= 2)
            printk("memstart = 0x%p, virt_addr = 0x%p\n",
                   (void *) bdp->mem_start,
                   (void *) Adapter->HardwareVirtualAddress);

        e1000_init(bdp);

        /* Printout the board configuration */
        e1000_print_brd_conf(bdp);

        /* init the basic stats stuff */
        ClearHwStatsCounters(Adapter);
        /* Update the statistics needed by the upper */
        UpdateStatsCounters(bdp);

        /* up the loop count( it's also the number of our boards found) */
        loop_cnt++;

        if (e1000_debug_level >= 2) {
            printk("dev = 0x%p ", dev);
            printk("  priv = 0x%p\n", dev->priv);
            printk("  irq = 0x%x ", dev->irq);
            printk("  next = 0x%p ", dev->next);
            printk("  flags = 0x%x\n", dev->flags);
            printk("  bdp = 0x%p\n", bdp);
            printk("  irq_level = 0x%x\n", bdp->irq_level);
        }
    }                            /* end of pci_find_class while loop */

    e1000boards = num_boards = loop_cnt;

    if (num_boards)
        return (0);
    else
        return (-ENODEV);
}

/* register e1000_probe as our initilization routine */
module_init(e1000_probe);

/* set some of the modules specific things here */
#ifdef MODULE
MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION("Intel(R) PRO/1000 Gigabit Ethernet driver");
MODULE_PARM(TxDescriptors, "1-8i");
MODULE_PARM(RxDescriptors, "1-8i");
MODULE_PARM(Jumbo, "1-8i");
MODULE_PARM(WaitForLink, "1-8i");
MODULE_PARM(AutoNeg, "1-8i");
MODULE_PARM(Speed, "1-8i");
MODULE_PARM(ForceDuplex, "1-8i");
MODULE_PARM(SBP, "1-8i");
#endif

/****************************************************************************
* Name:        cleanup_module
*
* Description: This routine is an entry point into the driver.
*
*               This is a Linux required routine.
*
* Author:      IntelCorporation
*
* Born on Date:    07/11/99
*
* Arguments:   
*       NONE
*
* Returns:
*        It returns 0  and can not fail
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
int
cleanup_module(void)
{
    bd_config_t *bdp, *next_bdp;
    PADAPTER_STRUCT Adapter;
    device_t *dev, *next_dev;

    /* start looking at the first device */
    if (e1000first)
        dev = e1000first->device;
    else
        return 0;
    if (e1000_debug_level >= 1)
        printk("cleanup_module: SOR, dev = 0x%p \n\n\n", dev);

    while (dev) {
#ifdef CONFIG_PROC_FS
        e1000_remove_proc_dev(dev);
#endif
        bdp = (bd_config_t *) dev->priv;
        next_bdp = bdp->bd_next;

        if (next_bdp)
            next_dev = next_bdp->device;
        else
            next_dev = NULL;
        Adapter = bdp->bddp;

        /* unregister this instance of the module */
        if (e1000_debug_level >= 2)
            printk("--Cleanup, unreg_netdev\n");
        unregister_netdev(dev);

        /* 
           * Free the memory mapped area that is allocated to the E1000 hardware
           * registers
         */
        if (e1000_debug_level >= 2)
            printk("--Cleanup, iounmap\n");
        iounmap(Adapter->HardwareVirtualAddress);
        /* free the irq back to the system */

#ifdef IANS
        kfree(bdp->iANSdata);
#endif
        /* free up any memory here */
        if (e1000_debug_level >= 2)
            printk("--Cleanup, e1000_dealloc_space\n");
        e1000_dealloc_space(bdp);

        dev = next_dev;
    }

#ifdef CONFIG_PROC_FS
    {
        struct proc_dir_entry   *de;

        /* check if the subdir list is empty before removing e1000_proc_dir */
        for (de = e1000_proc_dir->subdir; de; de = de->next) {
            /* ignore . and .. */
            if (*(de->name) == '.') continue;
            break;
        }
        if (de) return 0;
        remove_proc_entry(ADAPTERS_PROC_DIR, proc_net);
    }
#endif
    
    return (0);
}


/****************************************************************************
* Name:        e1000_check_options
*
* Description: This routine does range checking on command line options.
*
* Author:      IntelCorporation
*
* Born on Date:    03/28/2000
*
* Arguments:   
*        int board - board number to check values for
*
* Returns:
*        None
*
****************************************************************************/
static void
e1000_check_options(int board)
{

    /* Transmit Descriptor Count */
    if (TxDescriptors[board] == -1) {
        TxDescriptors[board] = MAX_TCB;
    } else if ((TxDescriptors[board] > E1000_MAX_TXD) ||
               (TxDescriptors[board] < E1000_MIN_TXD)) {
        printk("Invalid TxDescriptor count specified (%i),"
               " using default of %i\n", TxDescriptors[board], MAX_TCB);
        TxDescriptors[board] = MAX_TCB;
    } else {
        printk("Using specified value of %i TxDescriptors\n",
               TxDescriptors[board]);
    }

    /* Receive Descriptor Count */
    if (RxDescriptors[board] == -1) {
        RxDescriptors[board] = MAX_RFD;
    } else if ((RxDescriptors[board] > E1000_MAX_RXD) ||
               (RxDescriptors[board] < E1000_MIN_RXD)) {
        printk("Invalid RxDescriptor count specified (%i),"
               " using default of %i\n", RxDescriptors[board], MAX_RFD);
        RxDescriptors[board] = MAX_RFD;
    } else {
        printk("Using specified value of %i RxDescriptors\n",
               RxDescriptors[board]);
    }

    /* Jumbo Frame Enable */
    if (Jumbo[board] == -1) {
        Jumbo[board] = 1;
    } else if ((Jumbo[board] > 1) || (Jumbo[board] < 0)) {
        printk("Invalid Jumbo specified (%i), using default of %i\n",
               Jumbo[board], 1);
        Jumbo[board] = 1;
    } else {
        printk("Jumbo Frames %s\n",
               Jumbo[board] == 1 ? "Enabled" : "Disabled");
    }

    /* Wait for link at driver load */
    if (WaitForLink[board] == -1) {
        WaitForLink[board] = 1;
    } else if ((WaitForLink[board] > 1) || (WaitForLink[board] < 0)) {
        printk("Invalid WaitForLink specified (%i), using default of %i\n",
               WaitForLink[board], 1);
        WaitForLink[board] = 1;
    } else {
        printk("WaitForLink %s\n",
               WaitForLink[board] == 1 ? "Enabled" : "Disabled");
    }

     /* Forced Speed and Duplex */
    switch (Speed[board]) {
    case -1:
        Speed[board] = 0;
        switch (ForceDuplex[board]) {
        case -1:
            ForceDuplex[board] = 0;
            break;
        case 0:
            printk("Speed and Duplex Autonegotiation Enabled\n");
            break;
        case 1:
            printk("Warning: Half Duplex specified without Speed\n");
            printk("Using Autonegotiation at Half Duplex only\n");
            break;
        case 2:
            printk("Warning: Full Duplex specified without Speed\n");
            printk("Using Autonegotiation at Full Duplex only\n");
            break;
        default:
            printk("Invalid Duplex Specified (%i), Parameter Ignored\n",
                   ForceDuplex[board]);
            ForceDuplex[board] = 0;
            printk("Speed and Duplex Autonegotiation Enabled\n");
        }
        break;     
    case 0:
        switch (ForceDuplex[board]) {
        case -1:
        case 0:
            printk("Speed and Duplex Autonegotiation Enabled\n");
            ForceDuplex[board] = 0;
            break;
        case 1:
            printk("Warning: Half Duplex specified without Speed\n");
            printk("Using Autonegotiation at Half Duplex only\n");
            break;
        case 2:
            printk("Warning: Full Duplex specified without Speed\n");
            printk("Using Autonegotiation at Full Duplex only\n");
            break;
        default:
            printk("Invalid Duplex Specified (%i), Parameter Ignored\n",
                   ForceDuplex[board]);
            ForceDuplex[board] = 0;
            printk("Speed and Duplex Autonegotiation Enabled\n");
        }
        break;
    case 10:
    case 100:
        switch (ForceDuplex[board]) {
        case -1:
        case 0:
            printk("Warning: %i Mbps Speed specified without Duplex\n",
                   Speed[board]);
            printk("Using Autonegotiation at %i Mbps only\n", Speed[board]);
            ForceDuplex[board] = 0;
            break;
        case 1:
   printk("Forcing to %i Mbps Half Duplex\n", Speed[board]);
            break;
        case 2:
            printk("Forcing to %i Mbps Full Duplex\n", Speed[board]);
            break;
        default:
            printk("Invalid Duplex Specified (%i), Parameter Ignored\n",
                   ForceDuplex[board]);
            ForceDuplex[board] = 0;
            printk("Warning: %i Mbps Speed specified without Duplex\n",
                   Speed[board]);
            printk("Using Autonegotiation at %i Mbps only\n", Speed[board]);
        }
        break;
    case 1000:
        switch (ForceDuplex[board]) {
        case -1:
        case 0:
            printk("Warning: 1000 Mbps Speed specified without Duplex\n");
            printk("Using Autonegotiation at 1000 Mbps Full Duplex only\n");
            ForceDuplex[board] = 0;
            break;
        case 1:
            printk("Warning: Half Duplex is not supported at 1000 Mbps\n");
            printk("Using Autonegotiation at 1000 Mbps Full Duplex only\n");
            break;
        case 2:
            printk("Using Autonegotiation at 1000 Mbps Full Duplex only\n");
            break;
        default:
            printk("Invalid Duplex Specified (%i), Parameter Ignored\n",
                   ForceDuplex[board]);
            ForceDuplex[board] = 0;
            printk("Warning: 1000 Mbps Speed specified without Duplex\n");
            printk("Using Autonegotiation at 1000 Mbps Full Duplex only\n");
        }
        break;
    default:
        printk("Invalid Speed Specified (%i), Parameter Ignored\n",
               Speed[board]);
        Speed[board] = 0;
        switch (ForceDuplex[board]) {
        case -1:
        case 0:
            printk("Speed and Duplex Autonegotiation Enabled\n");
            ForceDuplex[board] = 0;
            break;
        case 1:
            printk("Warning: Half Duplex specified without Speed\n");
            printk("Using Autonegotiation at Half Duplex only\n");
            break;
        case 2:
            printk("Warning: Full Duplex specified without Speed\n");
            printk("Using Autonegotiation at Full Duplex only\n");
            break;
        default:
            printk("Invalid Duplex Specified (%i), Parameter Ignored\n",
                   ForceDuplex[board]);
            ForceDuplex[board] = 0;
            printk("Speed and Duplex Autonegotiation Enabled\n");
        }
    }

    if (AutoNeg[board] == -1) {
        AutoNeg[board] = 0x2F;
    } else { 
        if (AutoNeg[board] & ~0x2F) {
            printk("Invalid AutoNeg Specified (0x%X)\n", AutoNeg[board]);
            AutoNeg[board] = 0x2F;
        }
        printk("AutoNeg Advertising ");
        if(AutoNeg[board] & 0x20) {
            printk("1000/FD");
            if(AutoNeg[board] & 0x0F)
                printk(", ");
        }
        if(AutoNeg[board] & 0x08) {
            printk("100/FD");
            if(AutoNeg[board] & 0x07)
                printk(", ");
        }
        if(AutoNeg[board] & 0x04) {
            printk("100/HD");
            if(AutoNeg[board] & 0x03)
                printk(", ");
        }
        if(AutoNeg[board] & 0x02) {
            printk("10/FD");
            if(AutoNeg[board] & 0x01)
                printk(", ");
        }
        if(AutoNeg[board] & 0x01)
            printk("10/HD");
        printk("\n");
    }
}

/****************************************************************************
* Name:        e1000_open
*
* Description: This routine is the open call for the interface.
*
*               This is a Linux required routine.
*
* Author:      IntelCorporation
*
* Born on Date:    07/11/99
*
* Arguments:   
*       NONE
*
* Returns:
*        It returns 0 on success 
*         -EAGAIN on failure
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static int
e1000_open(device_t * dev)
{
    bd_config_t *bdp;
    PADAPTER_STRUCT Adapter = 0;
    int ret_val;
    printk("v0 e1000 Alignment: %p %p\n", 
	   &(Adapter->FirstTxDescriptor), &(Adapter->NumRxDescriptors));

    bdp = dev->priv;
    Adapter = bdp->bddp;

    if (e1000_debug_level >= 1)
        printk("open: SOR, bdp = 0x%p\n", bdp);

    /* make sure we have not already opened this interface */
    if(bdp->flags & BOARD_OPEN)
        return -EBUSY;
    
    if (e1000_init(bdp)) {
        return -ENOMEM;
    }
    if (e1000_runtime_init(bdp)) {
        return -ENOMEM;
    }
    Adapter->AdapterStopped = FALSE;
   
#ifdef IANS_BASE_VLAN_TAGGING
    /* on a close a global reset is issued to the hardware, 
     * so VLAN settings are lost and need to be re-set on open */
    if((IANS_BD_TAGGING_MODE)ANS_PRIVATE_DATA_FIELD(bdp)->tag_mode != IANS_BD_TAGGING_NONE)
        bd_ans_hw_EnableVLAN(bdp);
#endif
       
    if (request_irq(dev->irq, &e1000_intr, SA_SHIRQ, "e1000", dev)) {
        if (e1000_debug_level >= 1)
            printk("open: request_irq failed");

        return (-EAGAIN);
    }

    /* Check to see if promiscuous mode needs to be turned on */
    if (dev->flags & IFF_PROMISC) {
        /* turn  promisc mode on */
        ret_val = e1000_set_promisc(bdp, B_TRUE);
        bdp->flags |= PROMISCUOUS;
    } else {
        /* turn  promisc mode off */
        ret_val = e1000_set_promisc(bdp, B_FALSE);
        bdp->flags &= ~PROMISCUOUS;
    }

#ifdef MODULE
    /* up the mod use count used by the system */
    MOD_INC_USE_COUNT;
#endif

    /* setup and start the watchdog timer */
    init_timer(&bdp->timer_id);

    /* set the timer value for 2 sec( i.e. 200 10msec tics ) 
     * jiffies are mesured in tics and is equiv. to LBOLTS in Unix
     */
    bdp->timer_id.expires = bdp->timer_val = jiffies + 200;
    bdp->timer_id.data = (ulong_t) dev;
    bdp->timer_id.function = (void *) &e1000_watchdog;

    /* start the timer */
    add_timer(&bdp->timer_id);

    /* set the device flags */
    netif_start_queue(dev);

    /* enable interrupts */
    e1000EnableInterrupt(Adapter);

    /* init the basic stats stuff */
    ClearHwStatsCounters(Adapter);

    bdp->flags |= BOARD_OPEN;
    return (0);
}

/****************************************************************************
* Name:        e1000_close
*
* Description: This routine is an entry point into the driver.
*
*               This is a Linux required routine.
*
* Author:      IntelCorporation
*
* Born on Date:    07/11/99
*
* Arguments:   
*         device_t pointer
*
* Returns:
*        It returns 0 and can not fail.
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static int
e1000_close(device_t * dev)
{
    bd_config_t *bdp;
    PADAPTER_STRUCT Adapter;
    ushort_t status;
    int j;

    bdp = dev->priv;
    Adapter = bdp->bddp;

    /* set the device to not started */

    netif_stop_queue(dev);
    /* stop the hardware */

    /* Disable all possible interrupts */
    E1000_WRITE_REG(Imc, (0xffffffff));
    status = E1000_READ_REG(Icr);

    /* Reset the chip */
    AdapterStop(Adapter);

    /* kill the timer */
    del_timer(&bdp->timer_id);

    /* free the irq back to the system */
    if (e1000_debug_level >= 1)
        printk("E1000: close: free_irq\n");
    free_irq(dev->irq, dev);

    /*
     * Free up the transmit descriptor area 
     */
    if (e1000_debug_level >= 2)
        printk("--Cleanup, free tx descriptor area\n");
    free_contig(bdp->base_tx_tbds);
    bdp->base_tx_tbds = NULL;

    if(Adapter->TxSkBuffs)
      free_contig(Adapter->TxSkBuffs);

    /* 
     *  Free up the RX_SW_PACKET area also free any allocated 
     *  receive buffers
     */
    if (e1000_debug_level >= 2)
        printk("--Cleanup, free rx packet area + skbuffs\n");
    if(Adapter->RxSkBuffs){
        for (j = 0; j < Adapter->NumRxDescriptors; j++) {
            if (Adapter->RxSkBuffs[j]){
                if (e1000_debug_level >= 2)
                    printk(" -- kfree_skb\n");
                dev_kfree_skb(Adapter->RxSkBuffs[j]);
                Adapter->RxSkBuffs[j] = 0;
            }
        }
        free_contig(Adapter->RxSkBuffs);
    }

    /*
     * Free the receive descriptor area 
     */
    if (e1000_debug_level >= 2)
        printk("--Cleanup, free rx descriptor area\n");
    /* free_contig( Adapter->e1000_rbd_data ); */
    free_contig(bdp->base_rx_rbds);
    bdp->base_rx_rbds = NULL;
    
    bdp->flags &= ~BOARD_OPEN;

#ifdef MODULE
    /* adjust the mod use count */
    MOD_DEC_USE_COUNT;
#endif

    return (0);
}

/*
 * the send a packet, may poke at e1000 to force it to start tx
 */
  
static int
e1000_xmit_frame_aux(struct sk_buff *skb, device_t * dev, int poke)
{
    bd_config_t *bdp;
    PADAPTER_STRUCT Adapter;
    int lock_flag;
    int ret;

    bdp = dev->priv;
    Adapter = (PADAPTER_STRUCT) bdp->bddp;

    if (test_and_set_bit(0, (void*)&dev->tbusy) != 0) {
      return 1;
    }

    if (e1000_debug_level >= 3)
        printk("e1000_tx_frame\n");

    /* call the transmit routine */
#ifdef CLICK_POLLING
    if(SendBuffer(skb, bdp, poke, dev->polling)){
#else
    if(SendBuffer(skb, bdp, poke, 0)){
#endif
      ret = 0;
    } else {
#ifdef IANS
      if(bdp->iANSdata->iANS_status == IANS_COMMUNICATION_UP) {
        ans_notify(dev, IANS_IND_XMIT_QUEUE_FULL);
      }
#endif
      ret = 1;
    }

    if(bdp->tx_out_res == 0)
      clear_bit(0, (void*)&dev->tbusy);

    return (ret);
}

/****************************************************************************
* Name:        e1000_xmit_frame
*
* Description: This routine is called to transmit a frame.
*
*
* Author:      IntelCorporation
*
* Born on Date:    07/11/99
*
* Arguments:   
*         sb_buff   pointer
*         device_t pointer
*
* Returns:
*        It returns B_FALSE on success
*         B_TRUE on failure
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static int
e1000_xmit_frame(struct sk_buff *skb, device_t * dev)
{
  return e1000_xmit_frame_aux(skb, dev, 1);
}

/****************************************************************************
* Name:       SendBuffer
*
* Description: This routine physically sends the packet to the nic controller.
*
*
* Author:      IntelCorporation
*
* Born on Date:    07/11/99
*
* Arguments:   
*         TX_SW_PACKET   pointer
*         bd_config_t      pointer
*
* Returns:
*        It returns B_TRUE always and can not fail.
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static UINT
SendBuffer(struct sk_buff *skb, bd_config_t * bdp, int poke, int polling)
{
    PADAPTER_STRUCT Adapter;
    PE1000_TRANSMIT_DESCRIPTOR CurrentTxDescriptor, nxt;
    // net_device_stats_t *stats;
    int di;

    Adapter = bdp->bddp;
    // stats = &bdp->net_stats;

    CurrentTxDescriptor = Adapter->NextAvailTxDescriptor;

    /* Don't use the last descriptor! */
    if (CurrentTxDescriptor == Adapter->LastTxDescriptor)
      nxt = Adapter->FirstTxDescriptor;
    else
      nxt = CurrentTxDescriptor + 1;
    if(nxt == Adapter->OldestUsedTxDescriptor){
      printk("e1000: out of descs in Sendbuffer\n");
      return(0);
    }

    di = CurrentTxDescriptor - Adapter->FirstTxDescriptor;
    if(Adapter->TxSkBuffs[di])
      printk("e1000 oops di %d TxSkBuffs[di] %x\n",
             di,
             (di >= 0 && di < 80) ? Adapter->TxSkBuffs[di] : 0);
    Adapter->TxSkBuffs[di] = skb;

    CurrentTxDescriptor->BufferAddress = virt_to_bus(skb->data);
    
    CurrentTxDescriptor->Lower.DwordData = skb->len;

    /* zero out the status field in the descriptor. */
    CurrentTxDescriptor->Upper.DwordData = 0;

#ifdef IANS
    if(bdp->iANSdata->iANS_status == IANS_COMMUNICATION_UP) {
        if(bd_ans_os_Transmit(bdp, CurrentTxDescriptor, &skb)==BD_ANS_FAILURE) {
            dev_kfree_skb(skb);
            return B_FALSE;
        }
    }
#endif 

    Adapter->NextAvailTxDescriptor = nxt;

    CurrentTxDescriptor->Lower.DwordData |=
      (E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS);

#if 1
    /*
     * If there is a valid value for the transmit interrupt delay, set up the
     * delay in the descriptor field
     */
    if (Adapter->TxIntDelay)
      CurrentTxDescriptor->Lower.DwordData |= E1000_TXD_CMD_IDE;
#else
    /*
     * Ask for a transmit complete interrupt every 60 packets. RTM
     * This seems to be broken -- sometimes the tx complete
     * interrupts never happen -- sending waits until a receive
     * packet arrives. RTM Dec 22 2000.
     */
    if(polling==0 && Adapter->TxIntDelay){
      static int dctr;
      if(dctr++ > 60){
        dctr = 0;
        /* Don't delay for this packet! */
      } else {
        /* Delay tx complete interrupt for most packets. */
        CurrentTxDescriptor->Lower.DwordData |= E1000_TXD_CMD_IDE;
      }
    }
#endif

    /* Set the RS or the RPS bit by looking at the ReportTxEarly setting */
    if (Adapter->ReportTxEarly == 1)
        CurrentTxDescriptor->Lower.DwordData |= E1000_TXD_CMD_RS;
    else
        CurrentTxDescriptor->Lower.DwordData |= E1000_TXD_CMD_RPS;

    if (poke)
      /* Advance the Transmit Descriptor Tail (Tdt), this tells the 
       * E1000 that this frame is available to transmit. 
       */
      E1000_WRITE_REG(Tdt, (((unsigned long) Adapter->NextAvailTxDescriptor -
                             (unsigned long) Adapter->FirstTxDescriptor) >> 4));

    /* Could we queue another packet? */
    if (Adapter->NextAvailTxDescriptor == Adapter->LastTxDescriptor)
      nxt = Adapter->FirstTxDescriptor;
    else
      nxt = Adapter->NextAvailTxDescriptor + 1;
    if(nxt == Adapter->OldestUsedTxDescriptor)
      bdp->tx_out_res = 1;

    return(1);
}

/****************************************************************************
* Name:        e1000_get_stats
*
* Description: This routine is called when the OS wants the nic stats returned
*
* Author:      IntelCorporation
*
* Born on Date:    7/12/99
*
* Arguments:   
*        device_t dev         - the device stucture to return stats on
*
* Returns:
*         the address of the net_device_stats stucture for the device
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static struct net_device_stats *
e1000_get_stats(device_t * dev)
{
    bd_config_t *bdp = dev->priv;

    /* Statistics are updated from the watchdog - so just return them now */
    return (&bdp->net_stats);
}


/* this routine is to change the MTU size for jumbo frames */
/****************************************************************************
* Name:        e1000_change_mtu
*
* Description: This routine is called when the OS would like the driver to
*               change the MTU size.  This is used for jumbo frame support.
*
* Author:      IntelCorporation
*
* Born on Date:    7/12/98
*
* Arguments:   
*            device_t   pointer    -   pointer to the device
*            int               -   the new MTU size 
*
* Returns:
*       ???? 
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static int
e1000_change_mtu(device_t * dev, int new_mtu)
{
    bd_config_t *bdp;
    PADAPTER_STRUCT Adapter;

    bdp = (bd_config_t *) dev->priv;
    Adapter = bdp->bddp;

    if ((new_mtu < MINIMUM_ETHERNET_PACKET_SIZE - ENET_HEADER_SIZE) ||
        (new_mtu > MAX_JUMBO_FRAME_SIZE - ENET_HEADER_SIZE))
        return -EINVAL;

    if (new_mtu <= MAXIMUM_ETHERNET_PACKET_SIZE - ENET_HEADER_SIZE) {
        /* 802.x legal frame sizes */
        Adapter->LongPacket = FALSE;
        if (Adapter->RxBufferLen != E1000_RXBUFFER_2048) {
            Adapter->RxBufferLen = E1000_RXBUFFER_2048;
            if (dev->flags & IFF_UP) {
                e1000_close(dev);
                e1000_open(dev);
            }
        }
    } else if (Adapter->MacType < MAC_LIVENGOOD) {
        /* Jumbo frames not supported on 82542 hardware */
        printk("e1000: Jumbo frames not supported on 82542\n");
        return -EINVAL;
    } else if (Jumbo[Adapter->bd_number] != 1) {
        printk("e1000: Jumbo frames disabled\n");
        return -EINVAL;
    } else if (new_mtu <= (4096 - 256) - ENET_HEADER_SIZE) {
        /* 4k buffers */
        Adapter->LongPacket = TRUE;
        if (Adapter->RxBufferLen != E1000_RXBUFFER_4096) {
            Adapter->RxBufferLen = E1000_RXBUFFER_4096;
            if (dev->flags & IFF_UP) {
                e1000_close(dev);
                e1000_open(dev);

            }
        }
    } else if (new_mtu <= (8192 - 256) - ENET_HEADER_SIZE) {
        /* 8k buffers */
        Adapter->LongPacket = TRUE;
        if (Adapter->RxBufferLen != E1000_RXBUFFER_8192) {
            Adapter->RxBufferLen = E1000_RXBUFFER_8192;
            if (dev->flags & IFF_UP) {
                e1000_close(dev);
                e1000_open(dev);
            }
        }
    } else {
        /* 16k buffers */
        Adapter->LongPacket = TRUE;
        if (Adapter->RxBufferLen != E1000_RXBUFFER_16384) {
            Adapter->RxBufferLen = E1000_RXBUFFER_16384;
            if (dev->flags & IFF_UP) {
                e1000_close(dev);
                e1000_open(dev);
            }
        }
    }
    dev->mtu = new_mtu;
    return 0;
}

/****************************************************************************
* Name:        e1000_init
*
* Description: This routine is called when this driver is loaded. This is
*        the initialization routine which allocates memory, starts the
*        watchdog, & configures the adapter, determines the system
*        resources.
*
* Author:      IntelCorporation
*
* Born on Date:    1/18/98
*
* Arguments:   
*        NONE
*
* Returns:
*       NONE 
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static int
e1000_init(bd_config_t * bdp)
{
    PADAPTER_STRUCT Adapter;
    device_t *dev;
    uint16_t LineSpeed, FullDuplex;

    if (e1000_debug_level >= 1)
        printk("e1000_init()\n");

    dev = bdp->device;
    Adapter = (PADAPTER_STRUCT) bdp->bddp;

    /*
     * Disable interrupts on the E1000 card 
     */
    e1000DisableInterrupt(Adapter);

   /**** Reset and Initialize the E1000 *******/
    AdapterStop(Adapter);
    Adapter->AdapterStopped = FALSE;

    /* Validate the EEPROM */
    if (!ValidateEepromChecksum(Adapter)) {
        printk("e1000: The EEPROM checksum is not valid \n");
        return (1);
    }

    /* read the ethernet address from the eprom */
    ReadNodeAddress(Adapter, &Adapter->perm_node_address[0]);

    if (e1000_debug_level >= 2) {
        printk("Node addr is: ");
        printk("%x:", Adapter->perm_node_address[0]);
        printk("%x:", Adapter->perm_node_address[1]);
        printk("%x:", Adapter->perm_node_address[2]);
        printk("%x:", Adapter->perm_node_address[3]);
        printk("%x:", Adapter->perm_node_address[4]);
        printk("%x\n ", Adapter->perm_node_address[5]);
    }

    /* save off the perm node addr in the bdp */
    memcpy(bdp->eaddr.bytes, &Adapter->perm_node_address[0],
           DL_MAC_ADDR_LEN);

    /* tell the system what the address is... */
    memcpy(&dev->dev_addr[0], &Adapter->perm_node_address[0],
           DL_MAC_ADDR_LEN);

    /* Scan the chipset and determine whether to set the RS bit or
     * the RPS bit during transmits. On some systems with a fast chipset
     * (450NX), setting the RS bit may cause data corruption. Check the
     * space.c paratemer to see if the user has forced this setting or 
     * has let the software do the detection.
     */

    if ((e1000_ReportTxEarly == 0) || (e1000_ReportTxEarly == 1))
        Adapter->ReportTxEarly = e1000_ReportTxEarly;    /* User setting */
    else {                        /* let the software setect the chipset */
        if(Adapter->MacType < MAC_LIVENGOOD) {
            if (DetectKnownChipset(Adapter) == B_TRUE)
                Adapter->ReportTxEarly = 1;    /* Set the RS bit */
            else
                Adapter->ReportTxEarly = 0;    /* Set the RPS bit */
        } else
            Adapter->ReportTxEarly = 1;
    }


    Adapter->FlowControl = e1000_flow_ctrl;
    Adapter->RxPciPriority = e1000_rxpci_priority;

    /* Do the adapter initialization */
    if (!InitializeHardware(Adapter)) {
        printk("InitializeHardware Failed\n");
        return (1);
    }
    
    /* all initialization done, mark board as present */
    bdp->flags = BOARD_PRESENT;

    CheckForLink(Adapter);
    if(E1000_READ_REG(Status) & E1000_STATUS_LU) {
        Adapter->LinkIsActive = TRUE;
        GetSpeedAndDuplex(Adapter, &LineSpeed, &FullDuplex);
        Adapter->cur_line_speed = (uint32_t) LineSpeed;
        Adapter->FullDuplex = (uint32_t) FullDuplex;
#ifdef IANS
        Adapter->ans_link = IANS_STATUS_LINK_OK;
        Adapter->ans_speed = (uint32_t) LineSpeed;
        Adapter->ans_duplex = FullDuplex == FULL_DUPLEX ? 
            BD_ANS_DUPLEX_FULL : BD_ANS_DUPLEX_HALF;
#endif
    } else {
        Adapter->LinkIsActive = FALSE;
        Adapter->cur_line_speed = 0;
        Adapter->FullDuplex = 0;
#ifdef IANS
          Adapter->ans_link = IANS_STATUS_LINK_FAIL;
    Adapter->ans_speed = 0;
    Adapter->ans_duplex = 0;
#endif
    }

    if (e1000_debug_level >= 1)
        printk("e1000_init: end\n");

    return (0);
}

static int
e1000_runtime_init(bd_config_t * bdp)
{
    PADAPTER_STRUCT Adapter;
    device_t *dev;

    if (e1000_debug_level >= 1)
        printk("e1000_init()\n");

    dev = bdp->device;

    Adapter = (PADAPTER_STRUCT) bdp->bddp; /* Setup Shared Memory Structures */

    /* Make sure RxBufferLen is set to something */
    if (Adapter->RxBufferLen == 0) {
        Adapter->RxBufferLen = E1000_RXBUFFER_2048;
        Adapter->LongPacket = FALSE;
    }

    if (!e1000_sw_init(bdp)) {
        /*  Board is disabled because all memory structures
         *  could not be allocated
         */
        printk
            ("%s - Could not allocate the software mem structures\n",
             e1000_driver);
        bdp->flags = BOARD_DISABLED;
        return (1);
    }

    /* Setup and initialize the transmit structures. */
    SetupTransmitStructures(Adapter, B_TRUE);

    /* Setup and initialize the receive structures -- we can receive packets
     * off of the wire 
     */
    SetupReceiveStructures(bdp, TRUE, TRUE);

    return 0;
}

/****************************************************************************
* Name:        e1000_set_mac
*
* Description: This routine sets the ethernet address of the board
*
* Author:      IntelCorporation
*
* Born on Date:    07/11/99
*
* Arguments:   
*      bdp    - Ptr to this card's bd_config_t structure
*      eaddrp - Ptr to the new ethernet address
*
* Returns:
*      1  - If successful
*      0  - If not successful
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static int
e1000_set_mac(device_t * dev, void *p)
{

    bd_config_t *bdp;
    uint32_t HwLowAddress = 0;
    uint32_t HwHighAddress = 0;
    uint32_t RctlRegValue;
    uint16_t PciCommandWord;
    PADAPTER_STRUCT Adapter;
    uint32_t IntMask;

    struct sockaddr *addr = p;

    if (e1000_debug_level >= 1)
        printk("set_eaddr()\n");

    bdp = dev->priv;
    Adapter = bdp->bddp;

    RctlRegValue = E1000_READ_REG(Rctl);

    /* 
     * setup the MAC address by writing to RAR0 
     */

    if (Adapter->MacType == MAC_WISEMAN_2_0) {

        /* if MWI was enabled then dis able it before issueing the receive
           * reset to the hardware. 
         */
        if (Adapter->PciCommandWord && CMD_MEM_WRT_INVALIDATE) {
            PciCommandWord =
                Adapter->PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;

            WritePciConfigWord(PCI_COMMAND_REGISTER, &PciCommandWord);
        }

        /* reset receiver */
        E1000_WRITE_REG(Rctl, E1000_RCTL_RST);

        DelayInMilliseconds(5);    /* Allow receiver time to go in to reset */
    }

    memcpy(Adapter->CurrentNetAddress, addr->sa_data, DL_MAC_ADDR_LEN);

   /******************************************************************
    ** Setup the receive address (individual/node/network address).
    ******************************************************************/

    if (e1000_debug_level >= 2)
        printk("Programming IA into RAR[0]\n");

    HwLowAddress = (Adapter->CurrentNetAddress[0] |
                    (Adapter->CurrentNetAddress[1] << 8) |
                    (Adapter->CurrentNetAddress[2] << 16) |
                    (Adapter->CurrentNetAddress[3] << 24));

    HwHighAddress = (Adapter->CurrentNetAddress[4] |
                     (Adapter->CurrentNetAddress[5] << 8) | E1000_RAH_AV);

    E1000_WRITE_REG(Rar[0].Low, HwLowAddress);
    E1000_WRITE_REG(Rar[0].High, HwHighAddress);

    memcpy(bdp->eaddr.bytes, addr->sa_data, DL_MAC_ADDR_LEN);
    memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

    if (Adapter->MacType == MAC_WISEMAN_2_0) {

      /******************************************************************
       ** Take the receiver out of reset.
       ******************************************************************/

        E1000_WRITE_REG(Rctl, 0);

        DelayInMilliseconds(1);

        /* if MWI was enabled then reenable it after issueing the global
         * or receive reset to the hardware. 
         */

        if (Adapter->PciCommandWord && CMD_MEM_WRT_INVALIDATE) {
            WritePciConfigWord(PCI_COMMAND_REGISTER,
                               &Adapter->PciCommandWord);
        }

        IntMask = E1000_READ_REG(Ims);
        e1000DisableInterrupt(Adapter);

        /* Enable receiver */
        SetupReceiveStructures(bdp, FALSE, FALSE);

        E1000_WRITE_REG(Ims, IntMask);
        /* e1000EnableInterrupt( Adapter ); */
    }

    return (0);
}


/****************************************************************************
* Name:        e1000_print_brd_conf
*
* Description: This routine printd the board configuration.
*
* Author:      IntelCorporation
*
* Born on Date:    7/24/97
*
* Arguments:   
*      bdp    - Ptr to this card's DL_bdconfig structure
*
* Returns:
*      NONE
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static void
e1000_print_brd_conf(bd_config_t * bdp)
{

    PADAPTER_STRUCT Adapter = (PADAPTER_STRUCT) bdp->bddp;

    if(Adapter->LinkIsActive == TRUE)
        printk("%s: Mem:0x%p  IRQ:%d  Speed:%ld Mbps  Dx:%s FlowCtl:%02x\n\n",
               bdp->device->name, (void *) bdp->mem_start,
               bdp->irq_level, Adapter->cur_line_speed,
               Adapter->FullDuplex == FULL_DUPLEX ? "Full" : "Half",
               Adapter->FlowControl);
    else 
        printk("%s: Mem:0x%p  IRQ:%d  Speed:N/A  Dx:N/A\n\n", 
               bdp->device->name, (void *) bdp->mem_start, bdp->irq_level);
}

/*****************************************************************************
* Name:        SetupTransmitStructures
*
* Description: This routine initializes all of the transmit related
*              structures.  This includes the Transmit descriptors 
*              and the TX_SW_PACKETs structures.
*
*              NOTE -- The device must have been reset before this routine
*                      is called.
*
* Author:      IntelCorporation
*
* Born on Date:    6/14/97
*
* Arguments:
*      Adapter - A pointer to our context sensitive "Adapter" structure.
*      DebugPrint - print debug or not.  ( not used in this driver )
*
*
* Returns:
*      (none)
*
* Modification log:
* Date      Who  Description
* --------  ---  --------------------------------------------------------
*
*****************************************************************************/

static void
SetupTransmitStructures(PADAPTER_STRUCT Adapter, boolean_t DebugPrint)
{

    UINT i;

    if (e1000_debug_level >= 1)
        printk("SetupTransmitStructures\n");

   /**********************************************************************
    * Setup  TxSwPackets
    **********************************************************************/

    /* 
     * The transmit algorithm parameters like the append size
     * and copy size  which are used for the transmit algorithm and
     * are configurable. 
     */
    Adapter->TxIntDelay = e1000_txint_delay;

   /**************************************************
    *        Setup TX Descriptors
    ***************************************************/

    /* Initialize all of the Tx descriptors to zeros */
    memset((PVOID) Adapter->FirstTxDescriptor, 0,
           (sizeof(E1000_TRANSMIT_DESCRIPTOR)) *
           Adapter->NumTxDescriptors);

    /* Setup TX descriptor pointers */
    Adapter->NextAvailTxDescriptor = Adapter->FirstTxDescriptor;
    Adapter->OldestUsedTxDescriptor = Adapter->FirstTxDescriptor;

    /* Setup the Transmit Control Register (TCTL). */
    if (Adapter->FullDuplex) {
        E1000_WRITE_REG(Tctl, (E1000_TCTL_PSP | E1000_TCTL_EN |
                               (E1000_COLLISION_THRESHOLD <<
                                E1000_CT_SHIFT) |
                               (E1000_FDX_COLLISION_DISTANCE <<
                                E1000_COLD_SHIFT)));
    } else {
        E1000_WRITE_REG(Tctl, (E1000_TCTL_PSP | E1000_TCTL_EN |
                               (E1000_COLLISION_THRESHOLD <<
                                E1000_CT_SHIFT) |
                               (E1000_HDX_COLLISION_DISTANCE <<
                                E1000_COLD_SHIFT)));
    }

   /***********************************************************
    *   Setup Hardware TX Registers 
    ************************************************************/

    /* Setup HW Base and Length of Tx descriptor area */
    E1000_WRITE_REG(Tdbal,
                    virt_to_bus((void *) Adapter->FirstTxDescriptor));
    E1000_WRITE_REG(Tdbah, 0);

    E1000_WRITE_REG(Tdl, (Adapter->NumTxDescriptors *
                          sizeof(E1000_TRANSMIT_DESCRIPTOR)));

    /* Setup our HW Tx Head & Tail descriptor pointers */
    E1000_WRITE_REG(Tdh, 0);
    E1000_WRITE_REG(Tdt, 0);


    /* Zero out the Tx Queue State registers -- we don't use this mechanism. */
    if (Adapter->MacType < MAC_LIVENGOOD) {
        E1000_WRITE_REG(Tqsal, 0);
        E1000_WRITE_REG(Tqsah, 0);
    }
    /* Set the Transmit IPG register with default values.
     * Three values are used to determine when we transmit a packet on the 
     * wire: IPGT, IPGR1, & IPGR2.  IPGR1 & IPGR2 have no meaning in full 
     * duplex. Due to the state machine implimentation all values are 
     * effectivly 2 higher i.e. a setting of 10 causes the hardware to 
     * behave is if it were set to 1
     *  2. These settigs are in "byte times".  For example, an IPGT setting
     * of 10 plus the state machine delay of 2 is 12 byte time = 96 bit 
     * times. 
     */

    /* Set the Transmit IPG register with default values. */
    switch (Adapter->MacType) {
    case MAC_LIVENGOOD:
        if(Adapter->MediaType == MEDIA_TYPE_FIBER) {
            E1000_WRITE_REG(Tipg, DEFAULT_LVGD_TIPG_IPGT_FIBER |
                                  (DEFAULT_LVGD_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT) |
                                  (DEFAULT_LVGD_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT));
        } else {
            E1000_WRITE_REG(Tipg, DEFAULT_LVGD_TIPG_IPGT_COPPER |
                                  (DEFAULT_LVGD_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT) |
                                  (DEFAULT_LVGD_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT));
        }
           break;
    case MAC_WISEMAN_2_0:
    case MAC_WISEMAN_2_1:
    default:
        E1000_WRITE_REG(Tipg, DEFAULT_WSMN_TIPG_IPGT |
                              (DEFAULT_WSMN_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT) |
                              (DEFAULT_WSMN_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT));
        break;
    }
    /*
     * Set the Transmit Interrupt Delay register with the vaule for the 
     * transmit interrupt delay
     */
    E1000_WRITE_REG(Tidv, Adapter->TxIntDelay);
}

/*****************************************************************************
* Name:        SetupReceiveStructures
*
* Description: This routine initializes all of the receive related
*              structures.  This includes the receive descriptors, the
*              actual receive buffers, and the RX_SW_PACKET software structures.
*
*              NOTE -- The device must have been reset before this routine
*                      is called.
*
* Author:      IntelCorporation
*
* Born on Date:    6/27/97
*
* Arguments:
*      Adapter - A pointer to our context sensitive "Adapter" structure.
*
*      DebugPrint - A variable that indicates whether a "debug" version of
*          the driver should print debug strings to the terminal.
*
*      flag - indicates if the driver should indicate link
* Returns:
*      (none)
*
* Modification log:
* Date      Who  Description
* --------  ---  --------------------------------------------------------
*
*****************************************************************************/
static int
SetupReceiveStructures(bd_config_t *bdp, boolean_t DebugPrint,
                       boolean_t flag)
{
    PE1000_RECEIVE_DESCRIPTOR RxDescriptorPtr;
    UINT i;
    PADAPTER_STRUCT Adapter = bdp->bddp;
    uint32_t RegRctl = 0;

    if (e1000_debug_level >= 1)
        printk("SetupReceiveStructures\n");

    /*
     * Set the value of the maximum number of packets that will be processed
     * in the interrupt context from the file space.c
     */
    Adapter->MaxNumReceivePackets = RxDescriptors[Adapter->bd_number];;

    /*
     * Set the Receive interrupt delay and also the maximum number of times 
     * the transmit and receive interrupt are going to be processed from the
     * space.c file
     */
    Adapter->RxIntDelay = e1000_rxint_delay;
    Adapter->MaxDpcCount = e1000_maxdpc_count;

    /* Initialize all of the Rx descriptors to zeros */
    memset((PVOID) Adapter->FirstRxDescriptor, 0,
           (sizeof(E1000_RECEIVE_DESCRIPTOR)) * Adapter->NumRxDescriptors);

    for (i = 0, RxDescriptorPtr = Adapter->FirstRxDescriptor;
         i < Adapter->NumRxDescriptors;
         i++, RxDescriptorPtr++) {

        if (Adapter->RxSkBuffs[i] == NULL)
            printk
                ("SetupReceiveStructures rcv buffer memory not allocated");
        else {
            PVOID va = Adapter->RxSkBuffs[i]->tail;
            RxDescriptorPtr->BufferAddress = 0;
            RxDescriptorPtr->BufferAddress += virt_to_bus(va);
        }
    }


    /* Setup our descriptor pointers */
    Adapter->NextRxDescriptorToCheck = Adapter->FirstRxDescriptor;
    Adapter->NextRxIndexToFill = 0;

    /*
     * Set up all the RCTL fields
     */
    E1000_WRITE_REG(Rctl, 0);

    E1000_WRITE_REG(Rdtr0, (Adapter->RxIntDelay | E1000_RDT0_FPDB));

    /* Setup HW Base and Length of Rx descriptor area */
    E1000_WRITE_REG(Rdbal0,
                    virt_to_bus((void *) Adapter->FirstRxDescriptor));
    E1000_WRITE_REG(Rdbah0, 0);

    E1000_WRITE_REG(Rdlen0, (Adapter->NumRxDescriptors *
                             sizeof(E1000_RECEIVE_DESCRIPTOR)));

    /* Setup our HW Rx Head & Tail descriptor pointers */
    E1000_WRITE_REG(Rdh0, 0);
    E1000_WRITE_REG(Rdt0,
                    (((unsigned long) Adapter->LastRxDescriptor -
                      (unsigned long) Adapter->FirstRxDescriptor) >> 4));

    /* Zero out the registers associated with the second receive descriptor
     * ring, because we don't use that ring.
     */
    if (Adapter->MacType < MAC_LIVENGOOD) {
        E1000_WRITE_REG(Rdbal1, 0);
        E1000_WRITE_REG(Rdbah1, 0);
        E1000_WRITE_REG(Rdlen1, 0);
        E1000_WRITE_REG(Rdh1, 0);
        E1000_WRITE_REG(Rdt1, 0);
    }
    /* Setup the Receive Control Register (RCTL), and ENABLE the receiver.
     * The initial configuration is to: Enable the receiver, accept
     * broadcasts, discard bad packets (and long packets), disable VLAN filter
     * checking, set the receive descriptor minimum threshold size to 1/2,
     * and the receive buffer size to 2k. */

    RegRctl = E1000_RCTL_EN |         /* Enable Receive Unit */
              E1000_RCTL_BAM |        /* Accept Broadcast Packets */
              (Adapter->MulticastFilterType << E1000_RCTL_MO_SHIFT) |
              E1000_RCTL_RDMTS0_HALF |
              E1000_RCTL_LBM_NO;      /* Loopback Mode = none */

    switch (Adapter->RxBufferLen) {
    case E1000_RXBUFFER_2048:
    default:
        RegRctl |= E1000_RCTL_SZ_2048;
        break;
    case E1000_RXBUFFER_4096:
        RegRctl |= E1000_RCTL_SZ_4096 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
        break;
    case E1000_RXBUFFER_8192:
        RegRctl |= E1000_RCTL_SZ_8192 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
        break;
    case E1000_RXBUFFER_16384:
        RegRctl |= E1000_RCTL_SZ_16384 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
        break;
    }

    if(SBP[Adapter->bd_number] > 0) {
        RegRctl |= E1000_RCTL_SBP;
    }
    E1000_WRITE_REG(Rctl, RegRctl);
    
    /*
     * Check and report the status of the link
     */
    if (!(E1000_READ_REG(Status) & E1000_STATUS_LU) && (flag == TRUE))
        printk("e1000: %s Link is Down\n", bdp->device->name);
   
       Adapter->DoRxResetFlag = B_FALSE;

    return (0);
}

/*****************************************************************************
* Name:        UpdateStatsCounters
*
* Description: This routine will dump and reset the E10001000's internal
*              Statistics counters.  The current stats dump values will be
*              added to the "Adapter's" overall statistics.
*
* Author:      IntelCorporation
*
* Born on Date:    6/13/97
*
* Arguments:
*      Adapter - A pointer to our context sensitive "Adapter" structure.
*
* Returns:
*      (none)
*
* Modification log:
* Date      Who  Description
* --------  ---  --------------------------------------------------------
*
*****************************************************************************/

static VOID
UpdateStatsCounters(bd_config_t * bdp)
{

    PADAPTER_STRUCT Adapter;
    net_device_stats_t *stats;

    Adapter = bdp->bddp;
    stats = &bdp->net_stats;

    /* Add the values from the chip's statistics registers to the total values
     * that we keep in software.  These registers clear on read.
     */
    Adapter->RcvCrcErrors += E1000_READ_REG(Crcerrs);
    Adapter->RcvSymbolErrors += E1000_READ_REG(Symerrs);
    Adapter->RcvMissedPacketsErrors += E1000_READ_REG(Mpc);
    Adapter->DeferCount += E1000_READ_REG(Dc);
    Adapter->RcvSequenceErrors += E1000_READ_REG(Sec);
    Adapter->RcvLengthErrors += E1000_READ_REG(Rlec);
    Adapter->RcvXonFrame += E1000_READ_REG(Xonrxc);
    Adapter->TxXonFrame += E1000_READ_REG(Xontxc);
    Adapter->RcvXoffFrame += E1000_READ_REG(Xoffrxc);
    Adapter->TxXoffFrame += E1000_READ_REG(Xofftxc);
    Adapter->Rcv64 += E1000_READ_REG(Prc64);
    Adapter->Rcv65 += E1000_READ_REG(Prc127);
    Adapter->Rcv128 += E1000_READ_REG(Prc255);
    Adapter->Rcv256 += E1000_READ_REG(Prc511);
    Adapter->Rcv512 += E1000_READ_REG(Prc1023);
    Adapter->Rcv1024 += E1000_READ_REG(Prc1522);
    Adapter->GoodReceives += E1000_READ_REG(Gprc);
    Adapter->RcvBroadcastPkts += E1000_READ_REG(Bprc);
    Adapter->RcvMulticastPkts += E1000_READ_REG(Mprc);
    Adapter->GoodTransmits += E1000_READ_REG(Gptc);
    Adapter->Rnbc += E1000_READ_REG(Rnbc);
    Adapter->RcvUndersizeCnt += E1000_READ_REG(Ruc);
    Adapter->RcvFragment += E1000_READ_REG(Rfc);
    Adapter->RcvOversizeCnt += E1000_READ_REG(Roc);
    Adapter->RcvJabberCnt += E1000_READ_REG(Rjc);
    Adapter->TotPktRcv += E1000_READ_REG(Tpr);
    Adapter->TotPktTransmit += E1000_READ_REG(Tpt);
    Adapter->TrsPkt64 += E1000_READ_REG(Ptc64);
    Adapter->TrsPkt65 += E1000_READ_REG(Ptc127);
    Adapter->TrsPkt128 += E1000_READ_REG(Ptc255);
    Adapter->TrsPkt256 += E1000_READ_REG(Ptc511);
    Adapter->TrsPkt512 += E1000_READ_REG(Ptc1023);
    Adapter->TrsPkt1024 += E1000_READ_REG(Ptc1522);
    Adapter->TrsMulticastPkt += E1000_READ_REG(Mptc);
    Adapter->TrsBroadcastPkt += E1000_READ_REG(Bptc);
    Adapter->TxAbortExcessCollisions += E1000_READ_REG(Ecol);
    Adapter->TxLateCollisions += E1000_READ_REG(Latecol);
    Adapter->TotalCollisions += E1000_READ_REG(Colc);
    Adapter->SingleCollisions += E1000_READ_REG(Scc);
    Adapter->MultiCollisions += E1000_READ_REG(Mcc);
    Adapter->FCUnsupported += E1000_READ_REG(Fcruc);

    /* The byte count registers are 64-bits, to reduce the chance of overflow
     * The entire 64-bit register clears when the high 32-bits are read, so the
     * lower half must be read first 
     */
    Adapter->RcvGoodOct  += E1000_READ_REG(Gorl);
    Adapter->RcvGoodOct     += ((uint64_t) E1000_READ_REG(Gorh) << 32);
    Adapter->TrsGoodOct  += E1000_READ_REG(Gotl);
    Adapter->TrsGoodOct  += ((uint64_t) E1000_READ_REG(Goth) << 32);
    Adapter->RcvTotalOct += E1000_READ_REG(Torl);
    Adapter->RcvTotalOct += ((uint64_t) E1000_READ_REG(Torh) << 32);
    Adapter->TrsTotalOct += E1000_READ_REG(Totl);
    Adapter->TrsTotalOct += ((uint64_t) E1000_READ_REG(Toth) << 32);

    /* New statistics registers in the 82543 */
    if (Adapter->MacType >= MAC_LIVENGOOD) {
        Adapter->AlignmentErrors  += E1000_READ_REG(Algnerrc);
        Adapter->TotalRcvErrors   += E1000_READ_REG(Rxerrc);
        Adapter->TrsUnderRun      += E1000_READ_REG(Tuc);
        Adapter->TrsNoCRS         += E1000_READ_REG(Tncrs);
        Adapter->CarrierExtErrors += E1000_READ_REG(Cexterr);
        Adapter->RcvDMATooEarly   += E1000_READ_REG(Rutec);
    }

    /* Fill out the OS statistics structure */
    stats->rx_packets = Adapter->GoodReceives;     /* total pkts received     */
    stats->tx_packets = Adapter->GoodTransmits;    /* total pkts transmitted  */
    stats->rx_bytes   = Adapter->RcvGoodOct;       /* total bytes reveived    */
    stats->tx_bytes   = Adapter->TrsGoodOct;       /* total bytes transmitted */
    stats->multicast  = Adapter->RcvMulticastPkts; /* multicast pkts received */
    stats->collisions = Adapter->TotalCollisions;  /* total collision count   */
    /* Rx Errors */
    stats->rx_errors        = Adapter->TotalRcvErrors + Adapter->RcvCrcErrors +
                              Adapter->AlignmentErrors + 
                              Adapter->RcvLengthErrors +
                              Adapter->Rnbc + Adapter->RcvMissedPacketsErrors +
                              Adapter->CarrierExtErrors;
    stats->rx_dropped       = Adapter->Rnbc;
    stats->rx_length_errors = Adapter->RcvLengthErrors;
    stats->rx_crc_errors    = Adapter->RcvCrcErrors;
    stats->rx_frame_errors  = Adapter->AlignmentErrors;
    stats->rx_fifo_errors   = Adapter->RcvMissedPacketsErrors;
    stats->rx_missed_errors = Adapter->RcvMissedPacketsErrors;
    /* Tx Errors */
    stats->tx_errors  = Adapter->TxAbortExcessCollisions +
                        Adapter->TrsUnderRun +
                        Adapter->TxLateCollisions; 
    stats->tx_aborted_errors = Adapter->TxAbortExcessCollisions;
    stats->tx_fifo_errors    = Adapter->TrsUnderRun;
    stats->tx_window_errors  = Adapter->TxLateCollisions;
    /* Tx Dropped needs to be maintained elsewhere */
}

/****************************************************************************
*
* Name:            ReadNodeAddress
*
* Description:     Gets the node/Ethernet/Individual Address for this NIC
*                  from the EEPROM.
*
*                  Example EEPROM map:
*                              Word 0 = AA00
*                              Word 1 = 1100
*                              Word 2 = 3322
*
* Author:      IntelCorporation
*
* Born on Date:    3/30/98
*
* Arguments:
*      Adapter     Ptr to this card's adapter data structure
*
* Returns:
*      Status      RET_STATUS_SUCCESS = read valid address
*                  RET_STATUS_FAILURE = unable to read address
*
* Modification log:
* Date      Who  Description
* --------  ---  --------------------------------------------------------
***************************************************************************/
static int
ReadNodeAddress(IN PADAPTER_STRUCT Adapter, OUT PUCHAR NodeAddress)
{
    UINT i;
    USHORT EepromWordValue;

    DL_LOG(strlog(DL_ID, 0, 3, SL_TRACE, "ReadNodeAddress"));

    /* Read our node address from the EEPROM. */
    for (i = 0; i < NODE_ADDRESS_SIZE; i += 2) {
        /* Get word i from EEPROM */
        EepromWordValue = ReadEepromWord(IN Adapter, IN(USHORT)

                                         (EEPROM_NODE_ADDRESS_BYTE_0
                                          + (i / 2)));

        /* Save byte i */
        NodeAddress[i] = (UCHAR) EepromWordValue;

        /* Save byte i+1 */
        NodeAddress[i + 1] = (UCHAR) (EepromWordValue >> 8);
    }


    /* Our IA should not have the Multicast bit set */
    if (NodeAddress[0] & 0x1) {
        return (RET_STATUS_FAILURE);
    }

    memcpy(&Adapter->CurrentNetAddress[0],
           &Adapter->perm_node_address[0], DL_MAC_ADDR_LEN);
    return (RET_STATUS_SUCCESS);

}

/*****************************************************************************
* Name:        e1000_set_promisc
*
* Description: This routine sets the promiscous mode for receiving packets
*              that it is passed.
*
* Author:      IntelCorporation
*
* Born on Date:    7/7/97
*
* Arguments:
*      bd_config_t  - board struct
*      flag         - turn it on( B_TRUE ) or off ( B_FALSE )
*
* Returns:
*        B_FALSE   - if successful
*        B_TRUE   - if not 
*
* Modification log:
* Date      Who  Description
* --------  ---  --------------------------------------------------------
*
*****************************************************************************/
static int
e1000_set_promisc(bd_config_t * bdp, int flag)
{
    uint32_t RctlRegValue;
    uint32_t TempRctlReg;
    uint32_t NewRctlReg;
    PADAPTER_STRUCT Adapter;

    Adapter = (PADAPTER_STRUCT) bdp->bddp;

    /*
     * Save the original value in the RCTL register
     */
    RctlRegValue = E1000_READ_REG(Rctl);
    TempRctlReg = RctlRegValue;


    if (e1000_debug_level >= 1)
        printk("e1000_set_promiscuous, SOR");

    /* Check to see if we should set/clear the "Unicast Promiscuous" bit. */
    if (flag == B_TRUE) {
        /*
         * Since the promiscuous flag is set to TRUE, set both the unicast as
         * well as the multicast promiscuous mode on the RCTL
         */
        TempRctlReg |= (E1000_RCTL_UPE | E1000_RCTL_MPE | E1000_RCTL_BAM);
        E1000_WRITE_REG(Rctl, TempRctlReg);
        if (e1000_debug_level >= 1)
            printk("Promiscuous mode ON");
    } else {
        /*
         * Turn off both the unicast as well as the multicast promiscuous mode
         */
        TempRctlReg &= (~E1000_RCTL_UPE);
        TempRctlReg &= (~E1000_RCTL_MPE);
        E1000_WRITE_REG(Rctl, TempRctlReg);
        if (e1000_debug_level >= 1)
            printk("Promiscuous mode OFF");
    }


    /* Write the new filter back to the adapter, if it has changed */
    if (TempRctlReg != RctlRegValue) {
        E1000_WRITE_REG(Rctl, TempRctlReg);
    }

    NewRctlReg = E1000_READ_REG(Rctl);

    if (NewRctlReg != TempRctlReg)
        return (B_FALSE);
    else
        return (B_TRUE);
}

/*****************************************************************************
* Name:        e1000DisableInterrupt
*
* Description: This routine disables (masks) all interrupts on our adapter.
*
* Author:      IntelCorporation
*
* Born on Date:    8/25/97
*
* Arguments:
*              pointer to our "Adapter" structure.
*
* Returns:
*      (none)
*
* Modification log:
* Date      Who  Description
* --------  ---  --------------------------------------------------------
*
*****************************************************************************/

static void
e1000DisableInterrupt(PADAPTER_STRUCT Adapter)
{
    if (e1000_debug_level >= 1)
        printk("DisableInterrupts: SOR\n");

    /* 
     * Mask all adapter interrupts.
     */
    E1000_WRITE_REG(Imc, (0xffffffff & ~E1000_IMC_RXSEQ));

}


/*****************************************************************************
* Name:        e1000EnableInterrupt
*
* Description: This routine enables (un-masks) all interrupts on our adapter.
*
* Author:      IntelCorporation
*
* Born on Date:    8/25/97
*
* Arguments:
*              pointer to our "Adapter" structure.
*
* Returns:
*      (none)
*
* Modification log:
* Date      Who  Description
* --------  ---  --------------------------------------------------------
*
*****************************************************************************/

static void
e1000EnableInterrupt(PADAPTER_STRUCT Adapter)
{
    if (e1000_debug_level >= 1)
        printk("EnableInterrupts: SOR\n");

    /* Enable interrupts (SBL -- may want to enable different int bits..) */
    E1000_WRITE_REG(Ims, IMS_ENABLE_MASK);
}

/*****************************************************************************
* Name:        e1000_intr
*
* Description: This routine is the ISR for the e1000 board. It services
*        the RX & TX queues & starts the RU if it has stopped due
*        to no resources.
*
* Author:      IntelCorporation
*
* Born on Date:    8/25/97
*
* Arguments:
*      ivec    - The interrupt vector for this board.
*
* Returns:
*      (none)
*
* Modification log:
* Date      Who  Description
* --------  ---  --------------------------------------------------------
*
*****************************************************************************/

static void
e1000_intr(int irq, void *dev_inst, struct pt_regs *regs)
{
    /* note - regs is not used but required by the system */
    uint32_t IcrContents;
    PADAPTER_STRUCT Adapter;
    uint32_t CtrlRegValue, TxcwRegValue, RxcwRegValue;
    UINT DpcCount;
    device_t *dev;
    bd_config_t *bdp;

    dev = (device_t *) dev_inst;
    bdp = dev->priv;
    Adapter = (PADAPTER_STRUCT) bdp->bddp;

    if (e1000_debug_level >= 1)
        printk("e1000_intr: bdp = 0x%p\n ", bdp);

    if (!Adapter)
        return;

    /*
     * The board is present but not yet initialized. Since the board is not
     * initialized we do not process the interrupt
     */
    if (!bdp->flags || (bdp->flags & BOARD_DISABLED)) {
        return;
    }

#ifdef CLICK_POLLING
    if(dev->polling)
      printk("e1000_intr: polling!\n");
#endif

    /* get board status */
    if ((IcrContents = E1000_READ_REG(Icr))) {

#ifdef CLICK_POLLING
      if(dev->polling)
        printk("e1000_intr: icr %x\n", IcrContents);
#endif

        /* This card actually has interrupts to process */

        /* Disable Interrupts from this card */
        e1000DisableInterrupt(Adapter);

        /*
         * The Receive Sequence errors RXSEQ and the link status change LSC
         * are checked to detect that the cable has been pulled out. For
         * the 82542 silicon, the receive sequence errors interrupt
         * are an indication that cable is not connected. If there are 
         * sequence errors or if link status has changed, proceed to the
         * cable disconnect workaround
         */

        if (IcrContents & (E1000_ICR_RXSEQ|E1000_ICR_LSC|E1000_ICR_GPI_EN1)) {
            Adapter->GetLinkStatus =
            Adapter->LinkStatusChanged = TRUE;
            /* run the watchdog ASAP */
            mod_timer(&bdp->timer_id, jiffies);
        }
        if(IcrContents & E1000_ICR_GPI_EN1) {
            ReadPhyRegister(Adapter, PXN_INT_STATUS_REG, Adapter->PhyAddress);
        }
        if(Adapter->LinkStatusChanged && 
           Adapter->MediaType == MEDIA_TYPE_FIBER) {
            CtrlRegValue = E1000_READ_REG(Ctrl);
            TxcwRegValue = E1000_READ_REG(Txcw);
            RxcwRegValue = E1000_READ_REG(Rxcw);
            if((CtrlRegValue & E1000_CTRL_SWDPIN1) ||
               ((RxcwRegValue & E1000_RXCW_C) &&
                (TxcwRegValue & E1000_TXCW_ANE))) {
                
                E1000_WRITE_REG(Txcw, Adapter->TxcwRegValue);
                E1000_WRITE_REG(Ctrl, CtrlRegValue & ~E1000_CTRL_SLU);
                Adapter->AutoNegFailed = 0;
            }
            Adapter->LinkStatusChanged = FALSE;
        }
        if(Adapter->LinkStatusChanged && 
           Adapter->MediaType == MEDIA_TYPE_COPPER) {
            CheckForLink(Adapter);
        }

        DpcCount = Adapter->MaxDpcCount;

        /*
         * Since we are already in the interrupt context, we want to clean up 
         * as many transmit and receive packets that have been processed by
         * E1000 since taking interrupts is expensive. We keep a count called
         * the DpcCount which is the maximum number of times that we will try 
         * to clean up interrupts from within this interrupt context. If the
         * count is too high, it can lead to interrupt starvation
         */
        while (dev->polling == 0 && DpcCount > 0) {
            /* Process the receive interrupts to clean receive descriptors */
            if (e1000_debug_level >= 3)
                printk("intr: DpcCount = 0x%x\n ", DpcCount);

            ProcessReceiveInterrupts(bdp);

            /*
             * Process and clean up transmit interrupts
             */
            ProcessTransmitInterrupts(bdp, 1);

            DpcCount--;

            if (DpcCount) {
                if (!(E1000_READ_REG(Icr))) {
                    break;
                }
            }
        }

        /* Enable Interrupts */
#ifdef CLICK_POLLING
        if(dev->polling == 0)
          e1000EnableInterrupt(Adapter);
#else
        e1000EnableInterrupt(Adapter);
#endif

        /*
         * If there are any frames that have been queued, they are also 
         * Processed from this context. ??? We need to verify if there is
         * any benefit of doing this
         */
    }
    return;
}

/****************************************************************************
* Name:        ProcessTransmitInterrupts
*
* Description: This routine services the TX queues. It reclaims the
*        TCB's & TBD's & other resources used during the transmit
*        of this buffer. It is called from the ISR.
*
* Author:      IntelCorporation
*
* Born on Date:    8/7/97
*
* Arguments:   
*      bdp - Ptr to this card's DL_bdconfig structure
*
* Returns:
*   NONE 
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/

static struct sk_buff *
ProcessTransmitInterrupts(bd_config_t * bdp, int clean)
{
    PE1000_TRANSMIT_DESCRIPTOR td;
    PADAPTER_STRUCT Adapter;
    UINT NumTxSwPacketsCleaned = 0;
    device_t *dev;
    int lock_flag_tx, di, freed_some = 0;
    struct sk_buff *skb_head, *skb_last;
    skb_head = skb_last = 0;

    if (e1000_debug_level >= 3)
        printk("ProcessTransmitInterrupts, SOR\n");

    Adapter = (PADAPTER_STRUCT) bdp->bddp;
    dev = bdp->device;

    while(1){
        td = Adapter->OldestUsedTxDescriptor;
        if (td > Adapter->LastTxDescriptor)
            td -= Adapter->NumTxDescriptors;

        /* Quit if we've caught up with SendBuffer */
        if(td == Adapter->NextAvailTxDescriptor)
          break;
          
        /* Quit if board hasn't sent this packet. */
        if((td->Upper.Fields.TransmitStatus & E1000_TXD_STAT_DD) == 0)
          break;

        if (td == Adapter->LastTxDescriptor)
          Adapter->OldestUsedTxDescriptor = Adapter->FirstTxDescriptor;
        else
          Adapter->OldestUsedTxDescriptor = (td + 1);

        di = td - Adapter->FirstTxDescriptor;
	if (clean) 
	  dev_kfree_skb_irq(Adapter->TxSkBuffs[di]);
	else {
	  struct sk_buff *skb = Adapter->TxSkBuffs[di];
          if (skb_head == 0) {
            skb_head = skb;
            skb_last = skb;
            skb_last->next = NULL;
          } else {
            skb_last->next = skb;
            skb->next = NULL;
            skb_last = skb;
          }
#if __i386__ && HAVE_INTEL_CPU
          asm volatile("prefetcht0 %0" :: "m" (skb->head));
#endif
	}
        Adapter->TxSkBuffs[di] = 0;
        NumTxSwPacketsCleaned++;
        freed_some++;
    }

    /*
     * Clear tbusy if we freed some descriptors.
     */
    if (freed_some && bdp->tx_out_res){
      bdp->tx_out_res = 0;
#ifdef IANS
      if(bdp->iANSdata->iANS_status == IANS_COMMUNICATION_UP) {
        ans_notify(dev, IANS_IND_XMIT_QUEUE_READY);
      }
#endif
      clear_bit(0, (void*)&dev->tbusy);
#ifdef CLICK_POLLING
      if (dev->polling == 0) 
#endif
	netif_wake_queue(dev);
    }

    return skb_head;
}

/****************************************************************************
* Name:        e1000_set_multicast
*
* Description: This routine sets a list of multicast addresses
*        as requested by the DLPI module in the 128 * 32 MTA
*        vector table. It also calculates the appropriate hash
*        value and sets the bit in the vector table
*
* Author:      IntelCorporation
*
* Born on Date:    1/12/98
*
* Arguments:   
*      dev - Ptr to this card's dev structure
*
* Returns:
*    NONE
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static void
e1000_set_multi(device_t * dev)
{

    bd_config_t *bdp;
    PADAPTER_STRUCT Adapter;
    uint32_t TempRctlReg = 0;
    uint32_t HwLowAddress;
    uint32_t HwHighAddress;
    USHORT i, j;
    UINT HashValue = 0, HashReg, HashBit;
    UINT TempUint;
    int ret_val;
    PUCHAR MulticastBuffer;
    uint16_t PciCommandWord;
    uint32_t IntMask;
    struct dev_mc_list *mc_list;
    uchar_t *mc_buff;


    /* The data must be a multiple of the Ethernet address size. */

    if (e1000_debug_level >= 1)
        printk("e1000_set_multicast\n");

    bdp = dev->priv;
    Adapter = (PADAPTER_STRUCT) bdp->bddp;
    MulticastBuffer = Adapter->pmc_buff->MulticastBuffer;

    /* check to see if promiscuous mode should be enabled */
    if ((dev->flags & IFF_PROMISC) && !(bdp->flags & PROMISCUOUS)) {
        /* turn  promisc mode on */
        ret_val = e1000_set_promisc(bdp, B_TRUE);
        bdp->flags |= PROMISCUOUS;
    } else {
        /* turn  promisc mode off */
        ret_val = e1000_set_promisc(bdp, B_FALSE);
        bdp->flags &= ~PROMISCUOUS;
    }

#if 0                            /* we should try to enable this feature... */
    /* this is how you turn on receiving ALL multicast addrs */
    if ((dev->flags & IFF_ALLMULTI) && !(bdp->flags & ALL_MULTI)) {
        /* turn on  recv all multi addrs */
        TempRctlReg |= E1000_RCTL_MPE;
        E1000_WRITE_REG(Rctl, TempRctlReg);
        bdp->flags |= ALL_MULTI;
    } else {
        /* turn off  recv all multi addrs */
        TempRctlReg &= ~E1000_RCTL_MPE;
/* this next line with hang the load, so be careful... */
        E1000_WRITE_REG(Rctl, TempRctlReg);
        bdp->flags &= ~ALL_MULTI;
    }
#endif

    mc_buff = Adapter->pmc_buff->MulticastBuffer;
    /* Fill in the multicast addresses. */
    for (i = 0, mc_list = dev->mc_list; i < dev->mc_count;
         i++, mc_list = mc_list->next) {
        memcpy(mc_buff, (u8 *) & mc_list->dmi_addr, DL_MAC_ADDR_LEN);
        mc_buff += DL_MAC_ADDR_LEN;
    }
    /* reset the MulticastBuffer pointer */

    /* set the count in the Adapter struct */
    Adapter->NumberOfMcAddresses = dev->mc_count;


    /* adjust the count on addrs to bytes */
    Adapter->pmc_buff->mc_count = dev->mc_count * DL_MAC_ADDR_LEN;

    /* The E1000 has the ability to do perfect filtering of 16 addresses.
     * The driver uses one of the E1000's 16 receive address registers
     * for its node/network/mac/individual address.  So, we have room
     * for up to 15 multicast addresses in the CAM, additional MC
     * addresses are handled by the MTA (Multicast Table Array)
     */

    /* this just needs to be the count of how many addrs 
     * there are in the table 
     */
    TempUint = dev->mc_count;


    /* If we were using the MTA then it must be cleared */
    if (Adapter->NumberOfMcAddresses > E1000_RAR_ENTRIES - 1) {
        for (i = 0; i < E1000_NUM_MTA_REGISTERS; i++) {
            E1000_WRITE_REG(Mta[i], 0);
        }
    }

    /* If we are given more MC addresses than we can handle, then we'll
     * notify the OS that we can't accept any more MC addresses
     */

    if (TempUint > MAX_NUM_MULTICAST_ADDRESSES) {
        Adapter->NumberOfMcAddresses = MAX_NUM_MULTICAST_ADDRESSES;
    } else {
        /* Set the number of MC addresses that we are being requested to use */
        Adapter->NumberOfMcAddresses = TempUint;
    }

    /* Store the Rctl values to use for restoring */
    TempRctlReg = E1000_READ_REG(Rctl);

    if (Adapter->MacType == MAC_WISEMAN_2_0) {

        /* if MWI was enabled then disable it before issueing the global
         * reset to the hardware. 
         */
        if (Adapter->PciCommandWord && CMD_MEM_WRT_INVALIDATE) {
            PciCommandWord =
                Adapter->PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;

            WritePciConfigWord(PCI_COMMAND_REGISTER, &PciCommandWord);
        }

        /* The E1000 must be in reset before changing any RA registers.
         * Reset receive unit.  The chip will remain in the reset state
         * until software explicitly restarts it.
         */
        E1000_WRITE_REG(Rctl, E1000_RCTL_RST);

        DelayInMilliseconds(5);    /* Allow receiver time to go in to reset */
    }

   /**********************************************************************
    * Copy up to 15 MC addresses into the E1000's receive address
    * registers pairs (Ral Rah).  The first pair (Ral0 Rah0) is used
    * for our MAC/Node/Network address/IA.  If there are more than 15
    * multicast addresses then the additional addresses will be hashed
    * and the Multicast Table Array (MTA) will be used.
    **********************************************************************/
    for (i = 0, j = 1;
         (i < Adapter->NumberOfMcAddresses && j < E1000_RAR_ENTRIES);
         i++, j++) {

        /* HW expcets these in big endian so we reverse the byte order */
        HwLowAddress =
            (MulticastBuffer[i * ETH_LENGTH_OF_ADDRESS] |
             (MulticastBuffer[i * ETH_LENGTH_OF_ADDRESS + 1] << 8)
             | (MulticastBuffer[i * ETH_LENGTH_OF_ADDRESS + 2] <<
                16) |
             (MulticastBuffer[i * ETH_LENGTH_OF_ADDRESS + 3] << 24));

        /* HW expcets these in big endian so we reverse the byte order */
        HwHighAddress =
            (MulticastBuffer[i * ETH_LENGTH_OF_ADDRESS + 4] |
             (MulticastBuffer[i * ETH_LENGTH_OF_ADDRESS +
                              5] << 8) | E1000_RAH_AV);

        E1000_WRITE_REG(Rar[j].Low, HwLowAddress);
        E1000_WRITE_REG(Rar[j].High, HwHighAddress);

    }

    /* if the Receive Addresses Registers are not full then 
     * Clear the E1000_RAH_AV bit in other Receive Address Registers
     * NOTE: 'j' must be unchanged from the RAL/RAH loop
     */

    while (j < E1000_RAR_ENTRIES) {
        E1000_WRITE_REG(Rar[j].Low, 0);
        E1000_WRITE_REG(Rar[j].High, 0);
        j++;
    }

    /* hash each remaining (if any) MC address into the E1000's receive
     * multicast hash table.  NOTE: 'i' must be unchanged from the RAL/RAH loop
     */

    while (i < Adapter->NumberOfMcAddresses) {

        /* The portion of the address that is used for the hash table is
         * determined by the MulticastFilterType setting.            
         */

        switch (Adapter->MulticastFilterType) {
            /*  [0] [1] [2] [3] [4] [5] 
             *  01  AA  00  12  34  56
             *  LSB     MSB - According to H/W docs
             */

        case 0:                /* [47:36] i.e. 0x563 for above example address */
            HashValue =
                ((MulticastBuffer
                  [i * ETH_LENGTH_OF_ADDRESS +
                   4] >> 4) | (((USHORT) MulticastBuffer[i *
                                                         ETH_LENGTH_OF_ADDRESS
                                                         + 5]) << 4));
            break;

        case 1:                /* [46:35] i.e. 0xAC6 for above example address */
            HashValue =
                ((MulticastBuffer
                  [i * ETH_LENGTH_OF_ADDRESS +
                   4] >> 3) | (((USHORT) MulticastBuffer[i *
                                                         ETH_LENGTH_OF_ADDRESS
                                                         + 5]) << 5));
            break;

        case 2:                /* [45:34] i.e. 0x5D8 for above example address */
            HashValue =
                ((MulticastBuffer
                  [i * ETH_LENGTH_OF_ADDRESS +
                   4] >> 2) | (((USHORT) MulticastBuffer[i *
                                                         ETH_LENGTH_OF_ADDRESS
                                                         + 5]) << 6));
            break;

        case 3:                /* [43:32] i.e. 0x634 for above example address */
            HashValue =
                ((MulticastBuffer
                  [i * ETH_LENGTH_OF_ADDRESS + 4]) | (((USHORT)
                                                       MulticastBuffer
                                                       [i *
                                                        ETH_LENGTH_OF_ADDRESS
                                                        + 5]) << 8));
            break;

        }

      /**********************************************************
       * Set the corresponding bit in the Multicast Table Array.
       * Mta is treated like a bit vector of 4096 bit split in
       * to 128 registers of 32 bits each.  The Register to set
       * is in bits 11:5 of HashValue and the Bit within the
       * register to set is bits 4:0.
       ************************************************************/

        /* Bit number to set is in lower 5 bits */
        HashBit = HashValue & 0x1F;

        /* MTA register number is bits 11:5  */
        HashReg = (HashValue >> 5) & 0x7F;

        /* Read MTA entry */
        TempUint = E1000_READ_REG(Mta[(HashReg)]);

        /* Set hashed bit  */
        TempUint |= (1 << HashBit);

        /* Write new value */
        E1000_WRITE_REG(Mta[HashReg], TempUint);

        i++;
    }

    if (Adapter->MacType == MAC_WISEMAN_2_0) {

        /* if WMI was enabled then reenable it after issueing the global
         * or receive reset to the hardware. 
         */
        if (Adapter->PciCommandWord && CMD_MEM_WRT_INVALIDATE) {
            WritePciConfigWord(PCI_COMMAND_REGISTER,
                               &Adapter->PciCommandWord);
        }

        DelayInMilliseconds(5);

        /* Take receiver out of reset */
        E1000_WRITE_REG(Rctl, 0);
        /* clear E1000_RCTL_RST bit (and all others) */
        IntMask = E1000_READ_REG(Ims);
        e1000DisableInterrupt(Adapter);

        /* Enable receiver */
        SetupReceiveStructures(bdp, FALSE, FALSE);

        /* e1000EnableInterrupt( Adapter ); */
        E1000_WRITE_REG(Ims, IntMask);

        /* Restore Receive Control to state it was in prior to MC add request */
        E1000_WRITE_REG(Rctl, TempRctlReg);
    }


    return;
}



/****************************************************************************
* Name:        ProcessReceiveInterrupts
*
* Description: This routine processes the RX interrupt & services the 
*        RX queues. For each successful RFD, it allocates a
*        msg block & sends the msg upstream. The freed RFD is then
*        put at the end of the free list of RFD's.
*
* Author:      IntelCorporation
*
* Born on Date:    7/19/97
*
* Arguments:   
*      bdp - Ptr to this card's DL_bdconfig structure
*
* Returns:
*    NONE
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/

static void
ProcessReceiveInterrupts(bd_config_t * bdp)
{
    PADAPTER_STRUCT Adapter;
    device_t *dev;
    int bytes_srvcd;            /* # of bytes serviced */
    sk_buff_t *skb, *new_skb;
    /* Pointer to the receive descriptor being examined. */
    PE1000_RECEIVE_DESCRIPTOR CurrentDescriptor;
    PE1000_RECEIVE_DESCRIPTOR LastDescriptorProcessed;
    /* The amount of data received in the RBA (will be PacketSize +
     * 4 for the CRC).
     */
    USHORT Length;
    UINT Count = 0;
    BOOLEAN Status = FALSE;
    // net_device_stats_t *stats;

    if (e1000_debug_level >= 3)
        printk("ProcessReceiveInterrupts, SOR\n");

    Adapter = (PADAPTER_STRUCT) bdp->bddp;
    dev = bdp->device;
    // stats = &bdp->net_stats;

    bytes_srvcd = 0;

    CurrentDescriptor = Adapter->NextRxDescriptorToCheck;

    if (!((CurrentDescriptor->ReceiveStatus) & E1000_RXD_STAT_DD)) {
        if (e1000_debug_level >= 3)
            printk("Proc_rx_ints: Nothing to do!\n");
        /* don't send anything up. just clear the RFD */
        return;
    }

    /* Loop through the receive descriptors starting at the last known
     * descriptor owned by the hardware that begins a packet.
     */
    while ((CurrentDescriptor->ReceiveStatus & E1000_RXD_STAT_DD) &&
           (Count < Adapter->MaxNumReceivePackets)) {
        int di = CurrentDescriptor - Adapter->FirstRxDescriptor;

        /* Make sure this is also the last descriptor in the packet. */
        if (!(CurrentDescriptor->ReceiveStatus & E1000_RXD_STAT_EOP)) {
            /*
             * If the received packet has spanned multiple descriptors, ignore
             * and discard all the packets that do not have EOP set and proceed
             * to the next packet.
             */
            printk("Receive packet consumed mult buffers\n");

            new_skb = Adapter->RxSkBuffs[di];

        } else {                /* packet has EOP set - begin */
            /* 
             * Store the packet length. Take the CRC lenght out of the length 
             *  calculation.   
             */
            Length = CurrentDescriptor->Length - CRC_LENGTH;

            /*
             * Only the packets that are valid ethernet packets are processed and
             * sent up the stack.Normally, hardware will discard bad packets . 
             */
#ifdef IANS_BASE_VLAN_TAGGING
            if ((Length <= (Adapter->LongPacket == TRUE ? MAX_JUMBO_FRAME_SIZE :
                                                  MAXIMUM_ETHERNET_PACKET_SIZE))
                && (Length >= 
                    (bdp->iANSdata->tag_mode == IANS_BD_TAGGING_802_3AC ?
                     MINIMUM_ETHERNET_PACKET_SIZE - QTAG_SIZE :
                     MINIMUM_ETHERNET_PACKET_SIZE))
                && ((CurrentDescriptor->Errors == 0) || 
                    (SBP[Adapter->bd_number] > 0 && 
                     CurrentDescriptor->Errors == E1000_RXD_ERR_CE))) {
#else
            if ((Length <= (Adapter->LongPacket == TRUE ? MAX_JUMBO_FRAME_SIZE :
                                                  MAXIMUM_ETHERNET_PACKET_SIZE))
                && (Length >= MINIMUM_ETHERNET_PACKET_SIZE)
                && ((CurrentDescriptor->Errors == 0) || 
                    (SBP[Adapter->bd_number] > 0 && 
                     CurrentDescriptor->Errors == E1000_RXD_ERR_CE))) {
#endif
                Status = TRUE;

                /* allocate a mesg buff to copy the msg in */
                skb = Adapter->RxSkBuffs[di];
                if (skb == NULL) {
                    printk("ProcessReceiveInterrupts found a NULL skb\n");
                    break;
                }


                if (e1000_debug_level >= 3)
                    printk
                        ("ProcRecInts: skb = 0x%p, skb_data = 0x%p, len = 0x%x\n",
                         skb, skb->data, skb->len);
                /*
                 * Allocate a new receive buffer to replace the receive buffer that
                 *   is being sent up the stack.
                 */
#ifdef IANS
                new_skb = alloc_skb(Adapter->RxBufferLen + BD_ANS_INFO_SIZE, 
                                    GFP_ATOMIC);
#else
                new_skb = alloc_skb(Adapter->RxBufferLen + 2, GFP_ATOMIC);
#endif
                if (new_skb == NULL) {

                    /* 
                     * If the allob_physreq fails, we then try to copy the received
                     * packet into a message block that is allocated using the call
                     * allocb.
                     */
                    printk("!Proc_Rec_Ints cannot alloc_skb memory\n");

                    return;
                }

#ifdef IANS
                skb_reserve(new_skb, BD_ANS_INFO_SIZE);
#else
                skb_reserve(new_skb, 2);
#endif
                /*
                 * Adjust the skb and send the received packet up the stack
                 */

                /* adjust the skb internal pointers */
                /* Modifies skb->tail and skb->len */
                skb_put(skb, Length);

                /* CHECKSUM */
                /* set the checksum info */
                     skb->ip_summed = CHECKSUM_NONE;

#ifdef IANS
                if(bdp->iANSdata->iANS_status == IANS_COMMUNICATION_UP) {
                    if(bd_ans_os_Receive(bdp, CurrentDescriptor, skb) == 
                        BD_ANS_FAILURE) {
                        dev_kfree_skb_irq(skb);
                    } else {
                        /* pass the packet up the stack */
                        netif_rx(skb);
                    }
                } else {
                    /* set the protocol */
                    skb->protocol = eth_type_trans(skb, dev);
                    /* pass the packet up the stack */
                    netif_rx(skb);
                }
#else
                /* set the protocol */
                skb->protocol = eth_type_trans(skb, dev);
                /* pass the packet up the stack */
                netif_rx(skb);

                if (e1000_debug_level >= 3)
                    printk
                        ("ProcRecInts: After skb_put, new_skb = 0x%p\n",
                         new_skb);

#endif
            } else {
                printk("e1000: Bad Packet Length\n");
                skb = Adapter->RxSkBuffs[di];
                new_skb = skb;    /* Reuse the same old skb */

                if (e1000_debug_level >= 3)
                    printk("e1000: re-using same skb\n");

            }                    /* packet len is good/bad if */
        }                        /* packet has EOP set - end */

        /* Zero out the receive descriptors status  */
        CurrentDescriptor->ReceiveStatus = 0;

        /* set who this is from */
        new_skb->dev = dev;

        Adapter->RxSkBuffs[di] = new_skb;

        if (new_skb == 0){
            CurrentDescriptor->BufferAddress = 0;
        } else {
            e1000_rxfree_cnt++;

            CurrentDescriptor->BufferAddress = virt_to_bus(new_skb->tail);
        }

        /* Advance our pointers to the next descriptor (checking for wrap). */
        if (CurrentDescriptor == Adapter->LastRxDescriptor)
            Adapter->NextRxDescriptorToCheck = Adapter->FirstRxDescriptor;
        else
            Adapter->NextRxDescriptorToCheck++;

        LastDescriptorProcessed = CurrentDescriptor;
        CurrentDescriptor = Adapter->NextRxDescriptorToCheck;

        if (e1000_debug_level >= 3)
            printk("Next RX Desc to check is 0x%p\n", CurrentDescriptor);

        Count++;

        /* Advance the E1000's Receive Queue #0  "Tail Pointer". */
        E1000_WRITE_REG(Rdt0, (((uintptr_t) LastDescriptorProcessed -
                                (uintptr_t) Adapter->FirstRxDescriptor) >> 4));

        if (e1000_debug_level >= 3)
            printk("Rx Tail pointer is now 0x%lX\n",
                   (((uintptr_t) CurrentDescriptor -
                     (uintptr_t) Adapter->FirstRxDescriptor) >> 4));

    }                            /* while loop for all packets with DD set */

    if (e1000_debug_level >= 3)
        printk("ProcessReceiveInterrupts: done\n");
    return;
}

/****************************************************************************
* Name:        e1000_watchdog
*
* Description: This routine monitors the board activity. It also updates the 
*              statistics that are used by some tools.
*
* Author:      IntelCorporation
*
* Born on Date:    8/2/97
*
* Arguments:   NONE
*
* Returns:     NONE
* 
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/


static void
e1000_watchdog(device_t * dev)
{
    uint16_t LineSpeed, FullDuplex;
    bd_config_t *bdp;
    PADAPTER_STRUCT Adapter;

    bdp = dev->priv;
    Adapter = (PADAPTER_STRUCT) bdp->bddp;

    /*
     * the routines in the watchdog should only be executed if the board pointer
     * is valid, i.e the adapter is actually present.
     */
    if (bdp->flags & BOARD_PRESENT) {

        if(!Adapter->AdapterStopped) {
            CheckForLink(Adapter);
        }
        if(E1000_READ_REG(Status) & E1000_STATUS_LU) {
            GetSpeedAndDuplex(Adapter, &LineSpeed, &FullDuplex);
            Adapter->cur_line_speed = (uint32_t) LineSpeed;
            Adapter->FullDuplex = (uint32_t) FullDuplex;
#ifdef IANS
            Adapter->ans_link = IANS_STATUS_LINK_OK;
            Adapter->ans_speed = (uint32_t) LineSpeed;
            Adapter->ans_duplex = FullDuplex == FULL_DUPLEX ? 
                BD_ANS_DUPLEX_FULL : BD_ANS_DUPLEX_HALF;
#endif
            if(Adapter->LinkIsActive == FALSE) {
                printk("e1000: %s %ld Mbps %s Link is Up FlowCtl:%02x\n",
                       dev->name,
                        Adapter->cur_line_speed, 
                        Adapter->FullDuplex == FULL_DUPLEX ?
                        "Full Duplex" : "Half Duplex",
                       Adapter->FlowControl);
                Adapter->LinkIsActive = TRUE;
            }
        } else {
            Adapter->cur_line_speed = 0;
            Adapter->FullDuplex = 0;
#ifdef IANS
            Adapter->ans_link = IANS_STATUS_LINK_FAIL;
            Adapter->ans_speed = 0;
            Adapter->ans_duplex = 0;
#endif
            if(Adapter->LinkIsActive == TRUE) {
                printk("e1000: %s Link is Down\n", dev->name);
                Adapter->LinkIsActive = FALSE;
            }
        }
        
        /* Update the statistics needed by the upper */
        UpdateStatsCounters(bdp);

    }
#ifdef IANS
    if (bdp->iANSdata->reporting_mode == IANS_STATUS_REPORTING_ON) {
        bd_ans_os_Watchdog(dev, bdp);
    }
#endif
    /* reset the timer to 2 sec */
    mod_timer(&bdp->timer_id, jiffies + (2 * HZ));

    return;
}


/* pci.c */

/*****************************************************************************
* Name:        e1000_find_pci_device
*
* Description: This routine finds all PCI adapters that have the given Device
*              ID and Vendor ID.  It will store the IRQ, I/O, mem address,
*              node address, and slot information for each adapter in the
*              CardsFound structure.  This routine will also enable the bus
*              master bit on any of our adapters (some BIOS don't do this).
*
*
* Author:      IntelCorporation
*
* Born on Date:    2/1/98
*
* Arguments:
*      vendor_id - Vendor ID of our 82557 based adapter.
*      device_id - Device ID of our 82557 based adapter.
*
* Returns:
*      B_TRUE     - if found pci devices successfully
*      B_FALSE     - if no pci devices found
*
* Modification log:
* Date      Who  Description
* --------  ---  --------------------------------------------------------
*
*****************************************************************************/
static boolean_t
e1000_find_pci_device(pci_dev_t * pcid, PADAPTER_STRUCT Adapter)
{
    ulong_t PciTrdyTimeOut = 0;
    ulong_t PciRetryTimeOut = 0;
    int status = 0;
    uint_t PciCommandWord = 0;

    if (e1000_debug_level >= 1)
        printk("e1000_find_pci_device\n");

    if ((((ushort_t) pcid->vendor) == E1000_VENDOR_ID) &&
        ((((ushort_t) pcid->device) == WISEMAN_DEVICE_ID) ||
        (((ushort_t) pcid->device) == LIVENGOOD_FIBER_DEVICE_ID) ||
        (((ushort_t) pcid->device) == LIVENGOOD_COPPER_DEVICE_ID))) {

        if (e1000_debug_level >= 2)
            printk
                ("pcid is ours, vendor = 0x%x, device = 0x%x\n",
                 pcid->vendor, pcid->device);

        /* Read the values of the Trdy timeout and
           * also the retry count register
         */

        pci_read_config_byte(pcid,
                             PCI_TRDY_TIMEOUT_REGISTER,
                             (uchar_t *) & PciTrdyTimeOut);

        pci_read_config_byte(pcid,
                             PCI_RETRY_TIMEOUT_REGISTER,
                             (uchar_t *) & PciRetryTimeOut);

        pci_read_config_byte(pcid,
                             PCI_REVISION_ID,
                             (uchar_t *) & (Adapter->RevID));

        pci_read_config_word(pcid, PCI_SUBSYSTEM_ID,
                             (ushort_t *) & (Adapter->SubSystemId));

        pci_read_config_word(pcid,
                             PCI_SUBSYSTEM_VENDOR_ID,
                             (ushort_t *) & (Adapter->SubVendorId));

        pci_read_config_word(pcid,
                             PCI_COMMAND,
                             (ushort_t *) & (Adapter->PciCommandWord));

        PciCommandWord = Adapter->PciCommandWord;
        Adapter->DeviceNum = pcid->devfn;

        Adapter->VendorId = pcid->vendor;
        Adapter->DeviceId = pcid->device;

        if (e1000_debug_level >= 2) {
            printk("PciTrdyTimeOut = 0x%x\n", (uchar_t) PciTrdyTimeOut);
            printk("PciRetryTimeOut = 0x%x\n", (uchar_t) PciRetryTimeOut);
            printk("RevID = 0x%x\n", (uchar_t) Adapter->RevID);
            printk("SUBSYSTEM_ID = 0x%x\n", Adapter->SubSystemId);
            printk("SUBSYSTEM_VENDOR_ID = 0x%x\n", Adapter->SubVendorId);
            printk("PciCommandWord = 0x%x\n",
                   (ushort_t) Adapter->PciCommandWord);
            printk("DeviceNum = 0x%x\n", Adapter->DeviceNum);
        }
        if (e1000_pcimlt_override)
            pci_write_config_byte(pcid,
                                  PCI_LATENCY_TIMER,
                                  e1000_pcimlt_override);

        /* code insp #2 */
        if (!(e1000_pcimwi_enable)) {
            if (Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
                PciCommandWord =
                    Adapter->PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;
                pci_write_config_word(pcid,
                                      PCI_COMMAND_REGISTER,
                                      PciCommandWord);
            }
        }

        /* Set the Trdy timeout to zero if it is not
         * already so
         */
        if (PciTrdyTimeOut) {
            PciTrdyTimeOut = 0;
            pci_write_config_byte(pcid,
                                  PCI_TRDY_TIMEOUT_REGISTER,
                                  PciTrdyTimeOut);

            pci_read_config_byte(pcid,
                                 PCI_TRDY_TIMEOUT_REGISTER,
                                 (uchar_t *) & PciTrdyTimeOut);

            if (PciTrdyTimeOut)
                status = B_FALSE;
            else
                status = B_TRUE;

        } else
            status = B_TRUE;

        /* Set the retry timeout to zero if it is not
           * already so
         */
        if (PciRetryTimeOut) {
            PciRetryTimeOut = 0;
            pci_write_config_byte(pcid,
                                  PCI_RETRY_TIMEOUT_REGISTER,
                                  PciRetryTimeOut);

            pci_read_config_byte(pcid,
                                 PCI_RETRY_TIMEOUT_REGISTER,
                                 (uchar_t *) & PciRetryTimeOut);

            if (PciRetryTimeOut)
                status = B_FALSE;
            else
                status = B_TRUE;
        } else
            status = B_TRUE;
    }

    if (e1000_debug_level >= 1)
        printk("find_pci: end, status = 0x%x\n", status);

    return (status);

}


/****************************************************************************
* Name:        e1000_sw_init
*
* Description : This routine initializes all software structures needed by the
*               driver. Allocates the per board structure for storing adapter 
*               information. This routine also allocates all the transmit and
*               and receive related data structures for the driver. The memory-
*               mapped PCI BAR address space is also allocated in this routine.
*
* Author:      IntelCorporation
*
* Born on Date:    7/21/97
*
* Arguments:   
*    bdp - Pointer to the DLPI board structure.
*
* Returns:
*    B_TRUE  - if successfully initialized
*    B_FALSE - if S/W initialization failed
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/

static boolean_t
e1000_sw_init(bd_config_t * bdp)
{
    PADAPTER_STRUCT Adapter;    /* stores all adapter specific info */
    ulong_t mem_base;            /* Memory base address */
    uint_t bd_no;
    int i;

    if (e1000_debug_level >= 1)
        printk("e1000_sw_init: [brd %d] begin\n", bdp->bd_number);

    Adapter = bdp->bddp;

    mem_base = bdp->mem_start;
    bd_no = bdp->bd_number;

    /*
     * The board specific information like the memory base address, the IO base
     * address and the board number are also stored in the Adapter structure
     */
    Adapter->mem_base = mem_base;
    Adapter->bd_number = bd_no;

    if (e1000_debug_level >= 2)
        printk
            ("e1000 - PR0/1000 board located at memory address 0x%lx\n",
             mem_base);

    /* First we have to check our "NumTxDescriptors" and make sure it is
     *  a multiple of 8, because the E1000 requires this...       
     */
    TxDescriptors[Adapter->bd_number] += (REQ_TX_DESCRIPTOR_MULTIPLE - 1);
    TxDescriptors[Adapter->bd_number] &=
        (~(REQ_TX_DESCRIPTOR_MULTIPLE - 1));

    Adapter->NumTxDescriptors = TxDescriptors[Adapter->bd_number];

    if(!(Adapter->TxSkBuffs =
         (struct sk_buff **) malloc_contig(sizeof(struct sk_buff *) *
                                           TxDescriptors
                                           [Adapter->bd_number]))) {
      printk("e1000_sw_init TxSkBuffs alloc failed\n");
      return(0);
    }
    memset(Adapter->TxSkBuffs, 0,
           sizeof(struct sk_buff *) * TxDescriptors[Adapter->bd_number]);
           

    /*
     * Allocate memory for the transmit buffer descriptors that are shared
     * between the driver and the E1000. The driver uses these descriptors to
     * give packets to the hardware for transmission  and the hardware returns
     * status information in this memory area once the packet has been 
     * transmitted.
     */
    bdp->base_tx_tbds = (void *) kmalloc(
                                         (sizeof
                                          (E1000_TRANSMIT_DESCRIPTOR) *
                                          TxDescriptors
                                          [Adapter->bd_number]) + 4096,
                                         GFP_KERNEL);
    if (bdp->base_tx_tbds == NULL) {
        printk("e1000_sw_init e1000_rbd_data alloc failed\n");
        return 0;
    }
    Adapter->e1000_tbd_data = (PE1000_TRANSMIT_DESCRIPTOR)
        (((unsigned long) bdp->base_tx_tbds + 4096) & ~4096);

    if (e1000_debug_level >= 2)
        printk("tbd_data = 0x%p\n", Adapter->e1000_tbd_data);

    /*
     * Set the first transmit descriptor to the beginning of the allocated
     * memory area and the last descriptor to the end of the memory area
     */
    Adapter->FirstTxDescriptor = Adapter->e1000_tbd_data;

    Adapter->LastTxDescriptor =
        Adapter->FirstTxDescriptor + (Adapter->NumTxDescriptors - 1);

    /* First we have to check our "NumRxDescriptors" and make sure it is
     * a multiple of 8, because the E10001000 requires this...    
     */
    RxDescriptors[Adapter->bd_number] += (REQ_RX_DESCRIPTOR_MULTIPLE - 1);
    RxDescriptors[Adapter->bd_number] &=
        (~(REQ_RX_DESCRIPTOR_MULTIPLE - 1));

    Adapter->NumRxDescriptors = RxDescriptors[Adapter->bd_number];
    printk("adapter: %d rx descriptors\n", Adapter->NumRxDescriptors);

    if(!(Adapter->RxSkBuffs =
         (struct sk_buff **) malloc_contig(sizeof(struct sk_buff *) *
                                           RxDescriptors
                                           [Adapter->bd_number]))) {
      printk("e1000_sw_init RxSkBuffs alloc failed\n");
      return(0);
    }
    memset(Adapter->RxSkBuffs, 0,
           sizeof(struct sk_buff *) * RxDescriptors[Adapter->bd_number]);

   /*------------------------------------------------------------------------
    * Allocate memory for the receive descriptors (in shared memory), with
    * enough room left over to make sure that we can properly align the
    * structures.
    *---------------------------------------------------------------------- 
    */

    bdp->base_rx_rbds = (void *) kmalloc(
                                         (sizeof
                                          (E1000_RECEIVE_DESCRIPTOR) *
                                          RxDescriptors
                                          [Adapter->bd_number]) + 4096,
                                         GFP_KERNEL);
    if (bdp->base_rx_rbds == 0) {
        printk("e1000_sw_init e1000_rbd_data alloc failed\n");
        return 0;
    }
    Adapter->e1000_rbd_data = (PE1000_RECEIVE_DESCRIPTOR)
        (((unsigned long) bdp->base_rx_rbds + 4096) & ~4096);

    if (e1000_debug_level >= 2)
        printk("rbd_data = 0x%p\n", Adapter->e1000_rbd_data);

    Adapter->FirstRxDescriptor =
        (PE1000_RECEIVE_DESCRIPTOR) Adapter->e1000_rbd_data;

    Adapter->LastRxDescriptor =
        Adapter->FirstRxDescriptor + (Adapter->NumRxDescriptors - 1);

   /*------------------------------------------------------------------------
    * We've allocated receive buffer pointers, and receive descriptors. Now
    * Allocate a buffer for each descriptor, and zero the buffer.
    *---------------------------------------------------------------------- 
    */

   /*------------------------------------------------------------------------
    * For each receive buffer, the Physical address will be stored
    * in the RX_SW_PACKET's "PhysicalAddress" field, while the
    * virtual address will be stored in the "VirtualAddress" field.
    *----------------------------------------------------------------------- 
    */

    /* allocate a physreq structure for specifying receive buffer DMA
     * requirements. 
     */

    /*
     * Allocate an initial set of receive sk_buffs.
     */
    for (i = 0; i < Adapter->NumRxDescriptors; i++) {
      struct sk_buff *skb;

#ifdef IANS
        skb = alloc_skb(Adapter->RxBufferLen + BD_ANS_INFO_SIZE, 
                                    GFP_ATOMIC);
#else
        skb = alloc_skb(Adapter->RxBufferLen + 2, GFP_ATOMIC);
#endif
        if (skb == NULL) {
            printk("e1000_sw_init SetupReceiveStructures BIG PROBLEM\n");
            return 0;
        } else {
            /* make the 14 byte mac header align to 16 */
#ifdef IANS
            skb_reserve(skb, BD_ANS_INFO_SIZE);
#else
            skb_reserve(skb, 2);
#endif
            /* set who this is from - JR 9/10/99 */
            skb->dev = bdp->device;

            Adapter->RxSkBuffs[i] = skb;
        }
    }


    /*
     * Set up the multicast table data area
     */
/*
   Adapter->pmc_buff->MulticastBuffer =
         ((mltcst_cb_t *)&bdp->mc_data)->MulticastBuffer;
*/
    Adapter->pmc_buff = (mltcst_cb_t *) bdp->mc_data;

    if (e1000_debug_level >= 1)
        printk("pmc_buff = 0x%p\n", Adapter->pmc_buff);

    return 1;
}

/****************************************************************************
* Name:        e1000_alloc_space
*
* Description : This routine allocates paragraph ( 16 bit ) aligned memory for
*                the driver. Memory allocated is for the following structures
*               - BDP ( the main interface struct between dlpi and the driver )
*               - error count structure for adapter statistics
*
*               For the MP_VERSION of this driver, each sap structure returned
*               by this routine has a pre allocated synchornize var. Ditto for
*               BDP's allocated ( they have a spin lock in them ).
*
* Author:      IntelCorporation
*
* Born on Date:    7/11/97
*
* Arguments:   
*    None
*
* Returns:
*    TRUE  - if successfully allocated
*    FALSE - if S/W allocation failed
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static bd_config_t *
e1000_alloc_space(void)
{
    bd_config_t *bdp, *temp_bd;
    PADAPTER_STRUCT Adapter;    /* stores all adapter specific info */

    if (e1000_debug_level >= 1)
        printk("e1000_alloc_space: begin\n");

    /* allocate space for the DL_board_t structures */
    bdp = (bd_config_t *) kmalloc(sizeof(bd_config_t), GFP_KERNEL);

    if (bdp == NULL) {
        if (e1000_debug_level >= 2)
            printk("e1000_alloc_space e1000_config alloc failed\n");
        return NULL;
    }

    /* fill the bdp with zero */
    memset(bdp, 0x00, sizeof(*bdp));

    if (e1000_debug_level >= 2)
        printk("bdp = 0x%p\n", bdp);

    /* if first one, save it, else link it into the chain of bdp's */
    if (e1000first == NULL) {
        e1000first = bdp;
        bdp->bd_number = 0;
        bdp->bd_next = NULL;
        bdp->bd_prev = NULL;

        if (e1000_debug_level >= 2)
            printk("First one\n");

    } else {
        /* No, so find last in list and link the new one in */
        temp_bd = e1000first;
        bdp->bd_number = 1;        /* it is at least 1 */
        while (temp_bd->bd_next != NULL) {
            temp_bd = (bd_config_t *) temp_bd->bd_next;
            bdp->bd_number++;    /* set the board number */
        }
        temp_bd->bd_next = bdp;
        bdp->bd_next = NULL;
        bdp->bd_prev = temp_bd;
        if (e1000_debug_level >= 2)
            printk("Not first one\n");

    }

    /* 
     * Allocate the  Adapter sturcuture. The Adapter structure contains all the
     * information that is specific to that board instance. It contains pointers
     * to all the transmit and receive data structures for the board, all the 
     * PCI related information for the board, and all the statistical counters
     * associated with the board.
     */
    Adapter =
        (PADAPTER_STRUCT) kmalloc(sizeof(ADAPTER_STRUCT), GFP_KERNEL);
    if (Adapter == NULL) {
        if (e1000_debug_level >= 2)
            printk("e1000_sw_init Adapter structure alloc failed\n");
        return (NULL);
    }

    memset(Adapter, 0x00, sizeof(ADAPTER_STRUCT));

    /*
     * The Adapter pointer is made to point to the bddp ( Board dependent 
     * pointer) that is referenced off the bdp ( board ) structure.
     */
    bdp->bddp = Adapter;

    if (e1000_debug_level >= 2)
        printk("bdp->bddp = 0x%p\n", bdp->bddp);

    /* allocate space for multi-cast address space */
    if (!
        (bdp->mc_data =
         (pmltcst_cb_t) malloc_contig(sizeof(mltcst_cb_t)))) {
        if (e1000_debug_level >= 2)
            printk("e1000_alloc_space mc_data alloc failed\n");
        return NULL;
    }
    memset(bdp->mc_data, 0x00, sizeof(mltcst_cb_t));

    if (e1000_debug_level >= 2)
        printk("bdp->mc_data = 0x%p\n", bdp->mc_data);

    if (e1000_debug_level >= 1)
        printk("e1000_alloc_space: end\n");

    return (bdp);
}

/****************************************************************************
* Name:        e1000_dealloc_space
*
* Description : This routine frees all the memory allocated by "alloc_space".
*
* Author:      IntelCorporation
*
* Born on Date:    7/17/97
*
* Arguments:   
*    None
*
* Returns:
*    none
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static void
e1000_dealloc_space(bd_config_t * bdp)
{
    if (e1000_debug_level >= 1)
        printk("e1000_dealloc_space, bdp = 0x%p\n", bdp);
    if (bdp) {
        free_contig(bdp->mc_data);
        bdp->mc_data = NULL;

        free_contig(bdp->bddp);
        bdp->bddp = NULL;

        /* unlink the bdp from the linked list */
        if (bdp == e1000first) {
            e1000first = (bd_config_t *) bdp->bd_next;
            if (bdp->bd_next)
                ((bd_config_t *) bdp->bd_next)->bd_prev = NULL;
        } else {
            if (bdp->bd_next)
                ((bd_config_t *) bdp->bd_next)->bd_prev = bdp->bd_prev;
            if (bdp->bd_prev)
                ((bd_config_t *) bdp->bd_prev)->bd_next = bdp->bd_next;
        }
    }

    free_contig(bdp);
    bdp = NULL;

    return;
}

/****************************************************************************
* Name:        malloc_contig
*
* Description : This routine allocates a contigious chunk of memory of 
*                 specified size.
*
* Author:      IntelCorporation
*
* Born on Date:    7/18/97
*
* Arguments:   
*    size - Size of contigious memory required.
*
* Returns:
*    void ptr  - if successfully allocated
*    NULL       - if allocation failed
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static void *
malloc_contig(int size)
{
    void *mem;

    /* allocate memory */
    mem = kmalloc(size, GFP_KERNEL);
    if (!mem)
        return (NULL);

    return (mem);
}

/****************************************************************************
* Name:        free_contig
*
* Description : This routine frees up space previously allocated by 
*               malloc_contig.
*
* Author:      IntelCorporation
*
* Born on Date:    1/18/98
*
* Arguments:   
*    ptr  - Pointer to the memory to free.
*    size - Size of memory to free.
*
* Returns:
*    TRUE  - if successfully initialized
*    FALSE - if S/W initialization failed
*
* Modification log:
* Date      Who  Description
* --------  ---  -------------------------------------------------------- 
*
****************************************************************************/
static void
free_contig(void *ptr)
{
    if (ptr)
        kfree(ptr);

    ptr = NULL;
}

static int
e1000_GetBrandingMesg(uint16_t dev, uint16_t sub_ven, uint16_t sub_dev)
{
    char *mesg = NULL;
    int i = 0;

    /* Go through the list of all valid subsystem IDs */
    while (e1000_vendor_info_array[i].idstr != NULL) {
        /* Look for exact match on sub_dev and sub_ven */
        if ((e1000_vendor_info_array[i].dev == dev) &&
            (e1000_vendor_info_array[i].sub_ven == sub_ven) &&
            (e1000_vendor_info_array[i].sub_dev == sub_dev)) {
            mesg = e1000_vendor_info_array[i].idstr;
            break;
        } else if ((e1000_vendor_info_array[i].dev == dev) &&
                   (e1000_vendor_info_array[i].sub_ven == sub_ven) &&
                   (e1000_vendor_info_array[i].sub_dev == CATCHALL)) {
            mesg = e1000_vendor_info_array[i].idstr;
        } else if ((e1000_vendor_info_array[i].dev == dev) &&
                   (e1000_vendor_info_array[i].sub_ven == CATCHALL) &&
                   (mesg == NULL)) {
            mesg = e1000_vendor_info_array[i].idstr;
        }
        i++;
    }
    if (mesg != NULL) {
        strcpy(e1000id_string, mesg);
        return 1;
    } else
        return 0;
}

/* fxhw.c */
static boolean_t
DetectKnownChipset(PADAPTER_STRUCT Adapter)
{
    pci_dev_t *pcid;
    ulong_t DataPort;
    uchar_t SaveConfig;
    uchar_t TestConfig;

    SaveConfig = inb(CF2_SPACE_ENABLE_REGISTER);

    outb((uchar_t) CF2_SPACE_ENABLE_REGISTER, 0x0E);

    TestConfig = inb(CF2_SPACE_ENABLE_REGISTER);

    if (TestConfig == 0x0E) {

        outb((uchar_t) CF2_SPACE_ENABLE_REGISTER, SaveConfig);

        return FALSE;
    }


    if (
        (pcid =
         pci_find_device(PCI_VENDOR_ID_INTEL, INTEL_440BX_AGP, NULL))
        || (pcid = pci_find_device(PCI_VENDOR_ID_INTEL, INTEL_440BX, NULL))
        || (pcid = pci_find_device(PCI_VENDOR_ID_INTEL, INTEL_440GX, NULL))
        || (pcid = pci_find_device(PCI_VENDOR_ID_INTEL, INTEL_440FX, NULL))
        || (pcid =
            pci_find_device(PCI_VENDOR_ID_INTEL, INTEL_430TX, NULL))) {

        outb((uchar_t) CF2_SPACE_ENABLE_REGISTER, SaveConfig);

        if (e1000_debug_level >= 3)
            printk
                ("Found a known member of the 430 or 440 chipset family");

        return TRUE;
    }


    if (
        (pcid =
         pci_find_device(PCI_VENDOR_ID_INTEL, INTEL_450NX_PXB, NULL))) {

        pci_read_config_byte(pcid, PCI_REV_ID_REGISTER, (u8 *) & DataPort);

        if (e1000_debug_level >= 3)
            printk
                ("Found a 450NX chipset with PXB rev ID 0x%x\n",
                 (uchar_t) DataPort);

        outb((uchar_t) CF2_SPACE_ENABLE_REGISTER, SaveConfig);

        if (((uchar_t) DataPort) >= PXB_C0_REV_ID) {
            if (e1000_debug_level >= 3)
                printk("Found a 450NX chipset with C0 or later PXB");
            return FALSE;
        } else {
            if (e1000_debug_level >= 3)
                printk("Found a 450NX chipset with B1 or earlier PXB");
            return TRUE;
        }
    }

    if (
        (pcid =
         pci_find_device(PCI_VENDOR_ID_INTEL, INTEL_450KX_GX_PB, NULL))) {

        outb((uchar_t) CF2_SPACE_ENABLE_REGISTER, SaveConfig);

        if (e1000_debug_level >= 3)
            printk("Found a 450KX or 450GX chipset");

        return TRUE;
    }

    outb((uchar_t) CF2_SPACE_ENABLE_REGISTER, SaveConfig);

    if (e1000_debug_level >= 3)
        printk("Did not find a known chipset");

    return FALSE;
}

static int
e1000_poll_on(struct device *dev)
{
  unsigned long flags;
  bd_config_t *bdp = dev->priv;
  PADAPTER_STRUCT Adapter = (PADAPTER_STRUCT) bdp->bddp;

#ifdef CLICK_POLLING
  if (!dev->polling) {
    printk("e1000_poll_on\n");

    save_flags(flags);
    cli();

    dev->polling = 2;
    e1000DisableInterrupt(Adapter);

    restore_flags(flags);

    Adapter->NextRxIndexToFill =
      Adapter->NextRxDescriptorToCheck - Adapter->FirstRxDescriptor;
  }
#endif

  return 0;
}

static int
e1000_poll_off(struct device *dev)
{
  bd_config_t *bdp = dev->priv;
  PADAPTER_STRUCT Adapter = (PADAPTER_STRUCT) bdp->bddp;

#ifdef CLICK_POLLING
  if(dev->polling > 0){
    dev->polling = 0;
    e1000EnableInterrupt(Adapter);
    printk("e1000_poll_off\n");
  }
#endif

  return 0;
}

static struct sk_buff *
e1000_rx_poll(struct device *dev, int *want)
{
  bd_config_t *bdp = dev->priv;
  PADAPTER_STRUCT Adapter = (PADAPTER_STRUCT) bdp->bddp;
  int got = 0, di;
  struct sk_buff *skb_head = 0, *skb_last = 0;
  PE1000_RECEIVE_DESCRIPTOR CurrentDescriptor;
  PE1000_RECEIVE_DESCRIPTOR LastDescriptorProcessed;
  sk_buff_t *skb, *new_skb;
  USHORT Length;

  while(got < *want){
    CurrentDescriptor = Adapter->NextRxDescriptorToCheck;
    di = CurrentDescriptor - Adapter->FirstRxDescriptor;
    skb = Adapter->RxSkBuffs[di];
   
#if 1
    {
      PE1000_RECEIVE_DESCRIPTOR PrefetchDescriptor;
      sk_buff_t *next_skb;
      if (CurrentDescriptor == Adapter->LastRxDescriptor)
        PrefetchDescriptor = Adapter->FirstRxDescriptor;
      else
        PrefetchDescriptor = Adapter->NextRxDescriptorToCheck+1;
      next_skb = 
	Adapter->RxSkBuffs[PrefetchDescriptor-Adapter->FirstRxDescriptor];
      /* this does not seem to matter much */
#if __i386__ && HAVE_INTEL_CPU
      asm volatile("prefetcht0 %0" :: "m" (PrefetchDescriptor->ReceiveStatus));
#endif
    }
#endif

    if(skb == 0)
      break;
    if((CurrentDescriptor->ReceiveStatus & E1000_RXD_STAT_DD) == 0)
      break;
    if (!(CurrentDescriptor->ReceiveStatus & E1000_RXD_STAT_EOP)) {
      printk("Receive packet consumed mult buffers\n");
      if(skb)
        dev_kfree_skb(skb);
    } else {
      Length = CurrentDescriptor->Length - CRC_LENGTH;
      if ((Length <= (Adapter->LongPacket == TRUE ? MAX_JUMBO_FRAME_SIZE :
                      MAXIMUM_ETHERNET_PACKET_SIZE))
          && (Length >= MINIMUM_ETHERNET_PACKET_SIZE)
          && ((CurrentDescriptor->Errors == 0) || 
              (SBP[Adapter->bd_number] > 0 && 
               CurrentDescriptor->Errors == E1000_RXD_ERR_CE))) {
	/* this is cheating: instead of calling skb_put,
	 * we just assume we get the right size */
        /* skb_put(skb, Length); */
	{
	  unsigned char *tmp = skb->tail;
	  skb->tail += Length;
	  skb->len += Length;
        }

        skb->ip_summed = CHECKSUM_NONE;
        skb->protocol = eth_type_trans(skb, dev);

        if (got == 0) {
          skb_head = skb;
          skb_last = skb;
          skb_last->next = NULL;
        } else {
          skb_last->next = skb;
          skb->next = NULL;
          skb_last = skb;
        }

        got++;
      } else {
        printk("e1000: Bad Packet Length\n");
      }
    }

    Adapter->RxSkBuffs[di] = 0;

    if (CurrentDescriptor == Adapter->LastRxDescriptor)
      Adapter->NextRxDescriptorToCheck = Adapter->FirstRxDescriptor;
    else
      Adapter->NextRxDescriptorToCheck++;

    LastDescriptorProcessed = CurrentDescriptor;
    CurrentDescriptor = Adapter->NextRxDescriptorToCheck;
  }

 out:
  *want = got;
  return skb_head;
}

static int
e1000_tx_queue(struct device *dev, struct sk_buff *skb)
{
  int ret;

#ifdef CLICK_POLLING
  ret = e1000_xmit_frame_aux(skb, dev, dev->polling == 0);
#else
  ret = e1000_xmit_frame_aux(skb, dev, 1);
#endif
  return(ret);
}

static int
e1000_tx_start(struct device *dev)
{ 
  e1000_tx_eob(dev);
  return 0;
}

int 
e1000_rx_refill(struct device *dev, struct sk_buff **skbs)
{
  bd_config_t *bdp = dev->priv;
  PADAPTER_STRUCT Adapter = (PADAPTER_STRUCT) bdp->bddp;
  int nfilled = 0, di, rtn;
  PE1000_RECEIVE_DESCRIPTOR dp = 0;
  struct sk_buff *skb_list;

  // rtm check
  if (skbs == 0) 
    return (Adapter->NextRxDescriptorToCheck >= 
	    (Adapter->FirstRxDescriptor+Adapter->NextRxIndexToFill))
      ? (Adapter->NextRxDescriptorToCheck -
	 (Adapter->FirstRxDescriptor+Adapter->NextRxIndexToFill))
      : ((Adapter->NextRxDescriptorToCheck+Adapter->NumRxDescriptors) -
	 (Adapter->FirstRxDescriptor+Adapter->NextRxIndexToFill));

  skb_list = *skbs;
  di = Adapter->NextRxIndexToFill;
  while(Adapter->RxSkBuffs[di] == 0 && skb_list != 0L){
    struct sk_buff *skb = skb_list;
    skb_list = skb_list->next;
    // do not use skb_reserve because click expects
    // packets w/ (4,0) alignment: some net cards, like
    // tulip, cannot transfer data on non-4 byte
    // alignment
    // skb_reserve(skb, 2);
    skb->dev = dev;
    Adapter->RxSkBuffs[di] = skb;
    dp = Adapter->FirstRxDescriptor + di;
    dp->BufferAddress = virt_to_bus(skb->tail);
    dp->ReceiveStatus = 0;
    nfilled++;
    di++;
    if(di >= Adapter->NumRxDescriptors)
      di = 0;
  }
  if (skb_list == 0)
    *skbs = 0;
  else
    *skbs = skb_list;

  Adapter->NextRxIndexToFill = di;

  if(nfilled){
    /* Advance the E1000's Receive Queue #0  "Tail Pointer". */
    E1000_WRITE_REG(Rdt0, (((uintptr_t) dp -
                            (uintptr_t) Adapter->FirstRxDescriptor) >> 4));
  }

  // rtm check
  return (Adapter->NextRxDescriptorToCheck >= 
	  (Adapter->FirstRxDescriptor+Adapter->NextRxIndexToFill))
      ? (Adapter->NextRxDescriptorToCheck -
	 (Adapter->FirstRxDescriptor+Adapter->NextRxIndexToFill))
      : ((Adapter->NextRxDescriptorToCheck+Adapter->NumRxDescriptors) -
	 (Adapter->FirstRxDescriptor+Adapter->NextRxIndexToFill));
}

static struct sk_buff *
e1000_tx_clean(struct device *dev)
{
  bd_config_t *bdp = dev->priv;

  return ProcessTransmitInterrupts(bdp, 0);
}

static int
e1000_tx_eob(struct device *dev)
{
  PADAPTER_STRUCT Adapter;
  bd_config_t *bdp;

  bdp = dev->priv;
  Adapter = bdp->bddp;

  /* Advance the Transmit Descriptor Tail (Tdt), this tells the 
   * E1000 that this frame is available to transmit. 
   */
  E1000_WRITE_REG(Tdt, (((unsigned long) Adapter->NextAvailTxDescriptor -
                         (unsigned long) Adapter->FirstTxDescriptor) >> 4));
  return 0;
}


#ifdef MODULE
/* ENTRY POINTS */
EXPORT_SYMBOL(e1000_open);
EXPORT_SYMBOL(e1000_close);
EXPORT_SYMBOL(e1000_xmit_frame);
EXPORT_SYMBOL(e1000_probe);
EXPORT_SYMBOL(e1000_change_mtu);
EXPORT_SYMBOL(e1000_intr);
/* DEBUG */
#endif
