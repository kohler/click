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

/***************************************************************************/
/*       /proc File System Interaface Support Functions                    */
/***************************************************************************/

#include "e1000.h"
extern char e1000_driver[];
extern char e1000_version[];
extern char *e1000id_string;
#include "e1000_proc.h"
#include <linux/proc_fs.h>

struct proc_dir_entry *e1000_proc_dir;

static int e1000_generic_read(char *page, char **start, off_t off,
				              int count, int *eof)
{
	int len;

	len = strlen(page);
	page[len++] = '\n';

	if (len <= off + count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;
	return len;
}

static int e1000_read_ulong(char *page, char **start, off_t off,
			   int count, int *eof, unsigned long l)
{
	sprintf(page, "%lu", l);

	return e1000_generic_read(page, start, off, count, eof);
}

static int e1000_read_ulong_hex(char *page, char **start, off_t off,
				   int count, int *eof, unsigned long l)
{
	sprintf(page, "0x%04lx", l);

	return e1000_generic_read(page, start, off, count, eof);
}

static int e1000_read_hwaddr(char *page, char **start, off_t off,
				int count, int *eof, unsigned char *hwaddr)
{
	sprintf(page, "%02X:%02X:%02X:%02X:%02X:%02X",
			hwaddr[0], hwaddr[1], hwaddr[2],
			hwaddr[3], hwaddr[4], hwaddr[5]);

	return e1000_generic_read(page, start, off, count, eof);
}

/* need to check page boundaries !!! */
static int e1000_read_info(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	struct net_device_stats *stats = &bdp->net_stats;
	unsigned char *hwaddr;
	char *pagep = page;
	char *msg;

#if 0
	e1000_GetBrandingMesg(Adapter->DeviceId, Adapter->SubVendorId, 
	                      Adapter->SubSystemId);
	msg = e1000id_string;
	page += sprintf(page, "%-25s %s\n", DESCRIPTION_TAG, msg);
#endif
	page += sprintf(page, "%-25s %s\n", DRVR_NAME_TAG, e1000_driver);

	page += sprintf(page, "%-25s %s\n", DRVR_VERSION_TAG, e1000_version);

	page += sprintf(page, "%-25s 0x%04lx\n",
					PCI_VENDOR_TAG, (unsigned long) Adapter->VendorId);
	page += sprintf(page, "%-25s 0x%04lx\n",
					PCI_DEVICE_ID_TAG, (unsigned long) Adapter->DeviceId);
	page += sprintf(page, "%-25s 0x%04lx\n",
					PCI_SUBSYSTEM_VENDOR_TAG,
					(unsigned long) Adapter->SubVendorId);
	page += sprintf(page, "%-25s 0x%04lx\n",
					PCI_SUBSYSTEM_ID_TAG,
					(unsigned long) Adapter->SubSystemId);
	page += sprintf(page, "%-25s 0x%02lx\n",
					PCI_REVISION_ID_TAG,
					(unsigned long) Adapter->RevID);
	page += sprintf(page, "%-25s %lu\n",
					PCI_BUS_TAG,
					(unsigned long) (Adapter->pci_dev->bus->number));
	page += sprintf(page, "%-25s %lu\n",
					PCI_SLOT_TAG,
					(unsigned
					 long) (PCI_SLOT((Adapter->pci_dev->devfn))));
	page +=
		sprintf(page, "%-25s %lu\n", IRQ_TAG,
				(unsigned long) (bdp->irq_level));
	page +=
		sprintf(page, "%-25s %s\n", SYSTEM_DEVICE_NAME_TAG,
				bdp->device->name);

	hwaddr = bdp->device->dev_addr;
	page += sprintf(page, "%-25s %02X:%02X:%02X:%02X:%02X:%02X\n",
					CURRENT_HWADDR_TAG,
					hwaddr[0], hwaddr[1], hwaddr[2],
					hwaddr[3], hwaddr[4], hwaddr[5]);

	hwaddr = Adapter->perm_node_address;
	page += sprintf(page, "%-25s %02X:%02X:%02X:%02X:%02X:%02X\n",
					PERMANENT_HWADDR_TAG,
					hwaddr[0], hwaddr[1], hwaddr[2],
					hwaddr[3], hwaddr[4], hwaddr[5]);

	page += sprintf(page, "\n");

	if (Adapter->LinkIsActive == 1)
		msg = "up";
	else
		msg = "down";
	page += sprintf(page, "%-25s %s\n", LINK_TAG, msg);

	if (Adapter->cur_line_speed)
		page += sprintf(page, "%-25s %lu\n",
						SPEED_TAG,
						(unsigned long) (Adapter->cur_line_speed));
	else
		page += sprintf(page, "%-25s %s\n", SPEED_TAG, "N/A");

	msg = Adapter->FullDuplex == FULL_DUPLEX ? "full" :
		((Adapter->FullDuplex == 0) ? "N/A" : "half");
	page += sprintf(page, "%-25s %s\n", DUPLEX_TAG, msg);

	if (bdp->device->flags & IFF_UP)
		msg = "up";
	else
		msg = "down";
	page += sprintf(page, "%-25s %s\n", STATE_TAG, msg);

	page += sprintf(page, "\n");

	page += sprintf(page, "%-25s %lu\n",
					RX_PACKETS_TAG, (unsigned long) stats->rx_packets);
	page += sprintf(page, "%-25s %lu\n",
					TX_PACKETS_TAG, (unsigned long) stats->tx_packets);
	page += sprintf(page, "%-25s %lu\n",
					RX_BYTES_TAG, (unsigned long) stats->rx_bytes);
	page += sprintf(page, "%-25s %lu\n",
					TX_BYTES_TAG, (unsigned long) stats->tx_bytes);
	page += sprintf(page, "%-25s %lu\n",
					RX_ERRORS_TAG, (unsigned long) stats->rx_errors);
	page += sprintf(page, "%-25s %lu\n",
					TX_ERRORS_TAG, (unsigned long) stats->tx_errors);
	page += sprintf(page, "%-25s %lu\n",
					RX_DROPPED_TAG, (unsigned long) stats->rx_dropped);
	page += sprintf(page, "%-25s %lu\n",
					TX_DROPPED_TAG, (unsigned long) stats->tx_dropped);
	page += sprintf(page, "%-25s %lu\n",
					MULTICAST_TAG, (unsigned long) stats->multicast);
	page += sprintf(page, "%-25s %lu\n",
					COLLISIONS_TAG, (unsigned long) stats->collisions);
	page += sprintf(page, "%-25s %lu\n",
					RX_LENGTH_ERRORS_TAG,
					(unsigned long) stats->rx_length_errors);
	page += sprintf(page, "%-25s %lu\n",
					RX_OVER_ERRORS_TAG,
					(unsigned long) stats->rx_over_errors);
	page += sprintf(page, "%-25s %lu\n",
					RX_CRC_ERRORS_TAG,
					(unsigned long) stats->rx_crc_errors);
	page += sprintf(page, "%-25s %lu\n",
					RX_FRAME_ERRORS_TAG,
					(unsigned long) stats->rx_frame_errors);
	page += sprintf(page, "%-25s %lu\n",
					RX_FIFO_ERRORS_TAG,
					(unsigned long) stats->rx_fifo_errors);
	page += sprintf(page, "%-25s %lu\n",
					RX_MISSED_ERRORS_TAG,
					(unsigned long) stats->rx_missed_errors);
	page += sprintf(page, "%-25s %lu\n",
					TX_ABORTED_ERRORS_TAG,
					(unsigned long) stats->tx_aborted_errors);
	page += sprintf(page, "%-25s %lu\n",
					TX_CARRIER_ERRORS_TAG,
					(unsigned long) stats->tx_carrier_errors);
	page += sprintf(page, "%-25s %lu\n",
					TX_FIFO_ERRORS_TAG,
					(unsigned long) stats->tx_fifo_errors);
	page += sprintf(page, "%-25s %lu\n",
					TX_HEARTBEAT_ERRORS_TAG,
					(unsigned long) stats->tx_heartbeat_errors);
	page += sprintf(page, "%-25s %lu\n",
					TX_WINDOW_ERRORS_TAG,
					(unsigned long) stats->tx_window_errors);

	page += sprintf(page, "\n");

	/* 8254x specific stats */
	page += sprintf(page, "%-25s %lu\n",
					TX_LATE_COLL_TAG, Adapter->TxLateCollisions);
	page += sprintf(page, "%-25s %lu\n",
					TX_DEFERRED_TAG, Adapter->DeferCount);
	page += sprintf(page, "%-25s %lu\n",
					TX_SINGLE_COLL_TAG, Adapter->SingleCollisions);
	page += sprintf(page, "%-25s %lu\n",
					TX_MULTI_COLL_TAG, Adapter->MultiCollisions);
	page += sprintf(page, "%-25s %lu\n",
					RX_LONG_ERRORS_TAG, Adapter->RcvOversizeCnt);
	page += sprintf(page, "%-25s %lu\n",
					RX_SHORT_ERRORS_TAG, Adapter->RcvUndersizeCnt);
	/* The 82542 does not have an alignment error count register */
	/* ALGNERRC is only valid in MII mode at 10 or 100 Mbps */
	if(Adapter->MacType >= MAC_LIVENGOOD)
		page += sprintf(page, "%-25s %lu\n",
						RX_ALIGN_ERRORS_TAG, Adapter->AlignmentErrors);
	page += sprintf(page, "%-25s %lu\n",
					RX_XON_TAG, Adapter->RcvXonFrame);
	page += sprintf(page, "%-25s %lu\n",
					RX_XOFF_TAG, Adapter->RcvXoffFrame);
	page += sprintf(page, "%-25s %lu\n",
					TX_XON_TAG, Adapter->TxXonFrame);
	page += sprintf(page, "%-25s %lu\n",
					TX_XOFF_TAG, Adapter->TxXoffFrame);
	*page = 0;
	return e1000_generic_read(pagep, start, off, count, eof);
}

#if 0
static int e1000_read_descr(char *page, char **start, off_t off,
			   int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	char *msg;

	e1000_GetBrandingMesg(Adapter->DeviceId, Adapter->SubVendorId, 
	                      Adapter->SubSystemId);
	msg = e1000id_string;
	strncpy(page, msg, PAGE_SIZE);

	return e1000_generic_read(page, start, off, count, eof);
}
#endif

static int e1000_read_drvr_name(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	strncpy(page, e1000_driver, PAGE_SIZE);

	return e1000_generic_read(page, start, off, count, eof);
}

static int e1000_read_drvr_ver(char *page, char **start, off_t off,
				  int count, int *eof, void *data)
{
	strncpy(page, e1000_version, PAGE_SIZE);

	return e1000_generic_read(page, start, off, count, eof);
}

static int e1000_read_pci_vendor(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;

	return e1000_read_ulong_hex(page, start, off, count, eof,
						  (unsigned long) Adapter->VendorId);
}

static int e1000_read_pci_device(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;

	return e1000_read_ulong_hex(page, start, off, count, eof,
						  (unsigned long) Adapter->DeviceId);
}

static int e1000_read_pci_sub_vendor(char *page, char **start, off_t off,
						int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;

	return e1000_read_ulong_hex(page, start, off, count, eof,
						  (unsigned long) Adapter->SubVendorId);
}

static int e1000_read_pci_sub_device(char *page, char **start, off_t off,
						int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;

	return e1000_read_ulong_hex(page, start, off, count, eof,
						  (unsigned long) Adapter->SubSystemId);
}

static int e1000_read_pci_revision(char *page, char **start, off_t off,
					  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;

	return e1000_read_ulong_hex(page, start, off, count, eof,
						  (unsigned long) Adapter->RevID);
}

static int e1000_read_dev_name(char *page, char **start, off_t off,
				  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	strncpy(page, bdp->device->name, PAGE_SIZE);

	return e1000_generic_read(page, start, off, count, eof);
}

static int e1000_read_pci_bus(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) (Adapter->pci_dev->bus->number));
}

static int e1000_read_pci_slot(char *page, char **start, off_t off,
				  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned
					   long) (PCI_SLOT((Adapter->pci_dev->devfn))));
}

static int e1000_read_irq(char *page, char **start, off_t off,
			 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) (bdp->irq_level));
}

static int e1000_read_current_hwaddr(char *page, char **start, off_t off,
						int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	unsigned char *hwaddr = bdp->device->dev_addr;

	return e1000_read_hwaddr(page, start, off, count, eof, hwaddr);
}

static int e1000_read_permanent_hwaddr(char *page, char **start, off_t off,
						  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	unsigned char *hwaddr = Adapter->perm_node_address;

	return e1000_read_hwaddr(page, start, off, count, eof, hwaddr);
}

static int e1000_read_link_status(char *page, char **start, off_t off,
					 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;

	if (Adapter->LinkIsActive == 1)
		strncpy(page, "up", PAGE_SIZE);
	else
		strncpy(page, "down", PAGE_SIZE);

	return e1000_generic_read(page, start, off, count, eof);
}

static int e1000_read_speed(char *page, char **start, off_t off,
			   int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;

	if (Adapter->cur_line_speed)
		return e1000_read_ulong(page, start, off, count, eof,
						  (unsigned long) (Adapter->cur_line_speed));
	strncpy(page, "N/A", PAGE_SIZE);
	return e1000_generic_read(page, start, off, count, eof);
}

static int e1000_read_dplx_mode(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	char *dplx_mode;

	dplx_mode = Adapter->FullDuplex == FULL_DUPLEX ? "full" :
		((Adapter->FullDuplex == 0) ? "N/A" : "half");
	strncpy(page, dplx_mode, PAGE_SIZE);

	return e1000_generic_read(page, start, off, count, eof);
}

static int e1000_read_state(char *page, char **start, off_t off,
			   int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	if (bdp->device->flags & IFF_UP)
		strncpy(page, "up", PAGE_SIZE);
	else
		strncpy(page, "down", PAGE_SIZE);

	return e1000_generic_read(page, start, off, count, eof);
}

static int e1000_read_rx_packets(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_packets);
}

static int e1000_read_tx_packets(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.tx_packets);
}

static int e1000_read_rx_bytes(char *page, char **start, off_t off,
				  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_bytes);
}

static int e1000_read_tx_bytes(char *page, char **start, off_t off,
				  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.tx_bytes);
}

static int e1000_read_rx_errors(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_errors);
}

static int e1000_read_tx_errors(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_errors);
}

static int e1000_read_rx_dropped(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_dropped);
}

static int e1000_read_tx_dropped(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.tx_dropped);
}

static int e1000_read_rx_multicast_packets(char *page, char **start, off_t off,
							  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.multicast);
}

static int e1000_read_collisions(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.collisions);
}

static int e1000_read_rx_length_errors(char *page, char **start, off_t off,
						  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_length_errors);
}

static int e1000_read_rx_over_errors(char *page, char **start, off_t off,
						int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_over_errors);
}

static int e1000_read_rx_crc_errors(char *page, char **start, off_t off,
					   int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_crc_errors);
}

static int e1000_read_rx_frame_errors(char *page, char **start, off_t off,
						 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_frame_errors);
}

static int e1000_read_rx_fifo_errors(char *page, char **start, off_t off,
						int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_fifo_errors);
}

static int e1000_read_rx_missed_errors(char *page, char **start, off_t off,
						  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.rx_missed_errors);
}

static int e1000_read_tx_aborted_errors(char *page, char **start, off_t off,
						   int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.tx_aborted_errors);
}

static int e1000_read_tx_carrier_errors(char *page, char **start, off_t off,
						   int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.tx_carrier_errors);
}

static int e1000_read_tx_fifo_errors(char *page, char **start, off_t off,
						int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.tx_fifo_errors);
}

static int e1000_read_tx_heartbeat_errors(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.tx_heartbeat_errors);
}

static int e1000_read_tx_window_errors(char *page, char **start, off_t off,
						  int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;

	return e1000_read_ulong(page, start, off, count, eof,
					  (unsigned long) bdp->net_stats.tx_window_errors);
}

/* 8254x specific stats */
static int e1000_read_tx_late_coll(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->TxLateCollisions);
}

static int e1000_read_tx_defer_events(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->DeferCount);
}
static int e1000_read_tx_single_coll(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->SingleCollisions);
}
static int e1000_read_tx_multi_coll(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->MultiCollisions);
}
static int e1000_read_rx_oversize(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->RcvOversizeCnt);
}
static int e1000_read_rx_undersize(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->RcvUndersizeCnt);
}
static int e1000_read_rx_align_err(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->AlignmentErrors);
}
static int e1000_read_rx_xon(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->RcvXonFrame);
}
static int e1000_read_rx_xoff(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->RcvXoffFrame);
}
static int e1000_read_tx_xon(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->TxXonFrame);
}
static int e1000_read_tx_xoff(char *page, char **start, off_t off,
							 int count, int *eof, void *data)
{
	bd_config_t *bdp = (bd_config_t *) data;
	PADAPTER_STRUCT Adapter = bdp->bddp;
	return e1000_read_ulong(page, start, off, count, eof, Adapter->TxXoffFrame);
}

static struct proc_dir_entry *e1000_create_proc_read(char *name,
										bd_config_t * bdp,
										struct proc_dir_entry *parent,
										read_proc_t * read_proc)
{
	struct proc_dir_entry *pdep;

	if (!(pdep = create_proc_entry(name, S_IFREG, parent)))
		return NULL;
	pdep->read_proc = read_proc;
	pdep->data = bdp;
	return pdep;
}

int e1000_create_proc_dev(bd_config_t * bdp)
{
	struct proc_dir_entry *dev_dir;
	char info[256];
	int len;

	dev_dir = create_proc_entry(bdp->device->name, S_IFDIR, e1000_proc_dir);

	strncpy(info, bdp->device->name, sizeof(info));
	len = strlen(info);
	strncat(info + len, ".info", sizeof(info) - len);

	/* info */
	if (!(e1000_create_proc_read(info, bdp, e1000_proc_dir, e1000_read_info)))
		return -1;

#if 0
	/* description */
	if (!(e1000_create_proc_read(DESCRIPTION_TAG, bdp, dev_dir, e1000_read_descr)))
		return -1;
#endif
	/* driver name */
	if (!(e1000_create_proc_read(DRVR_NAME_TAG, bdp, dev_dir, e1000_read_drvr_name)))
		return -1;
	/* driver version */
	if (!(e1000_create_proc_read(DRVR_VERSION_TAG, bdp, dev_dir, e1000_read_drvr_ver)))
		return -1;
	/* pci vendor */
	if (!(e1000_create_proc_read(PCI_VENDOR_TAG, bdp, dev_dir, e1000_read_pci_vendor)))
		return -1;
	/* pci device id */
	if (!(e1000_create_proc_read(PCI_DEVICE_ID_TAG, bdp, dev_dir,
						   e1000_read_pci_device))) return -1;
	/* pci sub vendor */
	if (!(e1000_create_proc_read(PCI_SUBSYSTEM_VENDOR_TAG, bdp, dev_dir,
						   e1000_read_pci_sub_vendor))) return -1;
	/* pci sub device id */
	if (!(e1000_create_proc_read(PCI_SUBSYSTEM_ID_TAG, bdp, dev_dir,
						   e1000_read_pci_sub_device))) return -1;
	/* pci revision id */
	if (!(e1000_create_proc_read(PCI_REVISION_ID_TAG, bdp, dev_dir,
						   e1000_read_pci_revision))) return -1;
	/* device name */
	if (!(e1000_create_proc_read(SYSTEM_DEVICE_NAME_TAG, bdp, dev_dir,
						   e1000_read_dev_name))) return -1;
	/* pci bus */
	if (!(e1000_create_proc_read(PCI_BUS_TAG, bdp, dev_dir, e1000_read_pci_bus)))
		return -1;
	/* pci slot */
	if (!(e1000_create_proc_read(PCI_SLOT_TAG, bdp, dev_dir, e1000_read_pci_slot)))
		return -1;
	/* irq */
	if (!(e1000_create_proc_read(IRQ_TAG, bdp, dev_dir, e1000_read_irq)))
		return -1;
	/* current hwaddr */
	if (!(e1000_create_proc_read(CURRENT_HWADDR_TAG, bdp, dev_dir,
						   e1000_read_current_hwaddr))) return -1;
	/* permanent hwaddr */
	if (!(e1000_create_proc_read(PERMANENT_HWADDR_TAG, bdp, dev_dir,
						   e1000_read_permanent_hwaddr))) return -1;

	/* link status */
	if (!(e1000_create_proc_read(LINK_TAG, bdp, dev_dir, e1000_read_link_status)))
		return -1;
	/* speed */
	if (!(e1000_create_proc_read(SPEED_TAG, bdp, dev_dir, e1000_read_speed)))
		return -1;
	/* duplex mode */
	if (!(e1000_create_proc_read(DUPLEX_TAG, bdp, dev_dir, e1000_read_dplx_mode)))
		return -1;
	/* state */
	if (!(e1000_create_proc_read(STATE_TAG, bdp, dev_dir, e1000_read_state)))
		return 1;
	/* rx packets */
	if (!(e1000_create_proc_read(RX_PACKETS_TAG, bdp, dev_dir, e1000_read_rx_packets)))
		return 1;
	/* tx packets */
	if (!(e1000_create_proc_read(TX_PACKETS_TAG, bdp, dev_dir, e1000_read_tx_packets)))
		return 1;
	/* rx bytes */
	if (!(e1000_create_proc_read(RX_BYTES_TAG, bdp, dev_dir, e1000_read_rx_bytes)))
		return 1;
	/* tx bytes */
	if (!(e1000_create_proc_read(TX_BYTES_TAG, bdp, dev_dir, e1000_read_tx_bytes)))
		return 1;
	/* rx errors */
	if (!(e1000_create_proc_read(RX_ERRORS_TAG, bdp, dev_dir, e1000_read_rx_errors)))
		return 1;
	/* tx errors */
	if (!(e1000_create_proc_read(TX_ERRORS_TAG, bdp, dev_dir, e1000_read_tx_errors)))
		return 1;
	/* rx dropped */
	if (!(e1000_create_proc_read(RX_DROPPED_TAG, bdp, dev_dir, e1000_read_rx_dropped)))
		return 1;
	/* tx dropped */
	if (!(e1000_create_proc_read(TX_DROPPED_TAG, bdp, dev_dir, e1000_read_tx_dropped)))
		return 1;
	/* multicast packets */
	if (!(e1000_create_proc_read(MULTICAST_TAG, bdp, dev_dir, 
									e1000_read_rx_multicast_packets)))
		return 1;

	/* collisions */
	if (!(e1000_create_proc_read (COLLISIONS_TAG, bdp, dev_dir, e1000_read_collisions))) 
		return 1;
			 
	/* rx length errors */
	if (!(e1000_create_proc_read(RX_LENGTH_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_rx_length_errors))) return 1;
	/* rx over errors */
	if (!(e1000_create_proc_read(RX_OVER_ERRORS_TAG, bdp, dev_dir,
							   e1000_read_rx_over_errors))) return 1;
	/* rx crc errors */
	if (!(e1000_create_proc_read(RX_CRC_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_rx_crc_errors))) return 1;
	/* rx frame errors */
	if (!(e1000_create_proc_read(RX_FRAME_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_rx_frame_errors))) return 1;
	/* rx fifo errors */
	if (!(e1000_create_proc_read(RX_FIFO_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_rx_fifo_errors))) return 1;
	/* rx missed errors */
	if (!(e1000_create_proc_read(RX_MISSED_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_rx_missed_errors))) return 1;
	/* tx aborted errors */
	if (!(e1000_create_proc_read(TX_ABORTED_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_tx_aborted_errors))) return 1;
	/* tx carrier errors */
	if (!(e1000_create_proc_read(TX_CARRIER_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_tx_carrier_errors))) return 1;
	/* tx fifo errors */
	if (!(e1000_create_proc_read(TX_FIFO_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_tx_fifo_errors))) return 1;
	/* tx heartbeat errors */
	if (!(e1000_create_proc_read(TX_HEARTBEAT_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_tx_heartbeat_errors))) return 1;
	/* tx window errors */
	if (!(e1000_create_proc_read(TX_WINDOW_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_tx_window_errors))) return 1;

	if (!(e1000_create_proc_read(TX_LATE_COLL_TAG, bdp, dev_dir,
						   e1000_read_tx_late_coll))) return 1;
	if (!(e1000_create_proc_read(TX_DEFERRED_TAG, bdp, dev_dir,
						   e1000_read_tx_defer_events))) return 1;
	if (!(e1000_create_proc_read(TX_SINGLE_COLL_TAG, bdp, dev_dir,
						   e1000_read_tx_single_coll))) return 1;
	if (!(e1000_create_proc_read(TX_MULTI_COLL_TAG, bdp, dev_dir,
						   e1000_read_tx_multi_coll))) return 1;
	if (!(e1000_create_proc_read(RX_LONG_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_rx_oversize))) return 1;
	if (!(e1000_create_proc_read(RX_SHORT_ERRORS_TAG, bdp, dev_dir,
						   e1000_read_rx_undersize))) return 1;
	if(bdp->bddp->MacType >= MAC_LIVENGOOD)
		if (!(e1000_create_proc_read(RX_ALIGN_ERRORS_TAG, bdp, dev_dir,
							   e1000_read_rx_align_err))) return 1;
	if (!(e1000_create_proc_read(RX_XON_TAG, bdp, dev_dir,
						   e1000_read_rx_xon))) return 1;
	if (!(e1000_create_proc_read(RX_XOFF_TAG, bdp, dev_dir,
						   e1000_read_rx_xoff))) return 1;
	if (!(e1000_create_proc_read(TX_XON_TAG, bdp, dev_dir,
						   e1000_read_tx_xon))) return 1;
	if (!(e1000_create_proc_read(TX_XON_TAG, bdp, dev_dir,
						   e1000_read_tx_xoff))) return 1;

	return 0;
}

void e1000_remove_proc_dev(device_t * dev)
{
	struct proc_dir_entry *de;
	char info[256];
	int len;

	len = strlen(dev->name);
	strncpy(info, dev->name, sizeof(info));
	strncat(info + len, ".info", sizeof(info) - len);

	for (de = e1000_proc_dir->subdir; de; de = de->next) {
		if ((de->namelen == len) && (memcmp(de->name, dev->name, len)))
			break;
	}
	if (de) {
		remove_proc_entry(DESCRIPTION_TAG, de);
		remove_proc_entry(DRVR_NAME_TAG, de);
		remove_proc_entry(DRVR_VERSION_TAG, de);
		remove_proc_entry(PCI_VENDOR_TAG, de);
		remove_proc_entry(PCI_DEVICE_ID_TAG, de);
		remove_proc_entry(PCI_SUBSYSTEM_VENDOR_TAG, de);
		remove_proc_entry(PCI_SUBSYSTEM_ID_TAG, de);
		remove_proc_entry(PCI_REVISION_ID_TAG, de);
		remove_proc_entry(SYSTEM_DEVICE_NAME_TAG, de);
		remove_proc_entry(PCI_BUS_TAG, de);
		remove_proc_entry(PCI_SLOT_TAG, de);
		remove_proc_entry(IRQ_TAG, de);
		remove_proc_entry(CURRENT_HWADDR_TAG, de);
		remove_proc_entry(PERMANENT_HWADDR_TAG, de);

		remove_proc_entry(LINK_TAG, de);
		remove_proc_entry(SPEED_TAG, de);
		remove_proc_entry(DUPLEX_TAG, de);
		remove_proc_entry(STATE_TAG, de);

		remove_proc_entry(RX_PACKETS_TAG, de);
		remove_proc_entry(TX_PACKETS_TAG, de);
		remove_proc_entry(RX_BYTES_TAG, de);
		remove_proc_entry(TX_BYTES_TAG, de);
		remove_proc_entry(RX_ERRORS_TAG, de);
		remove_proc_entry(TX_ERRORS_TAG, de);
		remove_proc_entry(RX_DROPPED_TAG, de);
		remove_proc_entry(TX_DROPPED_TAG, de);
		remove_proc_entry(MULTICAST_TAG, de);
		remove_proc_entry(COLLISIONS_TAG, de);
		remove_proc_entry(RX_LENGTH_ERRORS_TAG, de);
		remove_proc_entry(RX_OVER_ERRORS_TAG, de);
		remove_proc_entry(RX_CRC_ERRORS_TAG, de);
		remove_proc_entry(RX_FRAME_ERRORS_TAG, de);
		remove_proc_entry(RX_FIFO_ERRORS_TAG, de);
		remove_proc_entry(RX_MISSED_ERRORS_TAG, de);
		remove_proc_entry(TX_ABORTED_ERRORS_TAG, de);
		remove_proc_entry(TX_CARRIER_ERRORS_TAG, de);
		remove_proc_entry(TX_FIFO_ERRORS_TAG, de);
		remove_proc_entry(TX_HEARTBEAT_ERRORS_TAG, de);
		remove_proc_entry(TX_WINDOW_ERRORS_TAG, de);
		remove_proc_entry(TX_LATE_COLL_TAG, de);
		remove_proc_entry(TX_DEFERRED_TAG, de);
		remove_proc_entry(TX_SINGLE_COLL_TAG, de);
		remove_proc_entry(TX_MULTI_COLL_TAG, de);
		remove_proc_entry(RX_LONG_ERRORS_TAG, de);
		remove_proc_entry(RX_SHORT_ERRORS_TAG, de);
		remove_proc_entry(RX_XON_TAG, de);
		remove_proc_entry(RX_XOFF_TAG, de);
		remove_proc_entry(TX_XON_TAG, de);
		remove_proc_entry(TX_XON_TAG, de);
	}
	remove_proc_entry(info, e1000_proc_dir);
	remove_proc_entry(dev->name, e1000_proc_dir);
}

