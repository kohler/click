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

/*
 * Proc fs support.
 *
 * Read-only files created by driver (if CONFIG_PROC_FS):
 *
 * /proc/net/PRO_LAN_Adapters/<ethx>.info
 * /proc/net/PRO_LAN_Adapters/<ethx>/<attribute>
 *
 * where <ethx>      is the system device name, i.e eth0.
 *       <attribute> is the driver attribute name.
 *
 * There is one file for each driver attribute, where the contents
 * of the file is the attribute value.  The ethx.info file contains
 * a list of all driver attributes in one file.
 *
 */

#include "e1000.h"

#ifdef CONFIG_PROC_FS

#include <linux/proc_fs.h>

#define ADAPTERS_PROC_DIR           "PRO_LAN_Adapters"
#define TAG_MAX_LENGTH              32
#define LINE_MAX_LENGTH             80
#define FIELD_MAX_LENGTH            LINE_MAX_LENGTH - TAG_MAX_LENGTH - 3

extern char e1000_driver_name[];
extern char e1000_driver_version[];

/*
 * The list of driver proc attributes is stored in a proc_list link
 * list.  The list is build with proc_list_setup and is used to
 * build the proc fs nodes.  The private data for each node is the
 * corresponding link in the link list.
 */

struct proc_list {
	struct list_head list;                  /* link list */
	char tag[TAG_MAX_LENGTH + 1];           /* attribute name */
	void *data;                             /* attribute data */
	size_t len;				/* sizeof data */
	char *(*func)(void *, size_t, char *);  /* format data func */
};

static int
e1000_proc_read(char *page, char **start, off_t off, int count, int *eof)
{
	int len = strlen(page);

	page[len++] = '\n';

	if(len <= off + count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if(len > count)
		len = count;
	if(len < 0)
		len = 0;

	return len;
}

static int
e1000_proc_info_read(char *page, char **start, off_t off,
                     int count, int *eof, void *data)
{
	struct list_head *proc_list_head = data, *curr;
	struct proc_list *elem;
	char *p = page;
	char buf[FIELD_MAX_LENGTH + 1];

	list_for_each(curr, proc_list_head) {
		elem = list_entry(curr, struct proc_list, list);

		if (p - page + LINE_MAX_LENGTH >= PAGE_SIZE)
			break;

		if(!strlen(elem->tag))
			p += sprintf(p, "\n");
		else
			p += sprintf(p, "%-*.*s %.*s\n", 
				TAG_MAX_LENGTH, TAG_MAX_LENGTH,
				elem->tag, FIELD_MAX_LENGTH,
				elem->func(elem->data, elem->len, buf));
	}

	*p = '\0';

	return e1000_proc_read(page, start, off, count, eof);
}

static int
e1000_proc_single_read(char *page, char **start, off_t off,
                       int count, int *eof, void *data)
{
	struct proc_list *elem = data;

	sprintf(page, "%.*s", FIELD_MAX_LENGTH, elem->func(elem->data, 
	        elem->len, page));

	return e1000_proc_read(page, start, off, count, eof);
}

static void __devexit
e1000_proc_dirs_free(char *name, struct list_head *proc_list_head)
{
	struct proc_dir_entry *intel_proc_dir, *proc_dir;
	char info_name[strlen(name) + strlen(".info")];

	for(intel_proc_dir = proc_net->subdir; intel_proc_dir;
		intel_proc_dir = intel_proc_dir->next) {
		if((intel_proc_dir->namelen == strlen(ADAPTERS_PROC_DIR)) &&
		   !memcmp(intel_proc_dir->name, ADAPTERS_PROC_DIR, strlen(ADAPTERS_PROC_DIR)))
			break;
	}

	if(!intel_proc_dir)
		return;

	for(proc_dir = intel_proc_dir->subdir; proc_dir;
		proc_dir = proc_dir->next) {
		if ((proc_dir->namelen == strlen(name)) &&
		    !memcmp(proc_dir->name, name, strlen(name)))
			break;
	}

	if(proc_dir) {
		struct list_head *curr;
		struct proc_list *elem;

		list_for_each(curr, proc_list_head) {
			elem = list_entry(curr, struct proc_list, list);
			remove_proc_entry(elem->tag, proc_dir);
		}

		strcpy(info_name, name);
		strcat(info_name, ".info");

		remove_proc_entry(info_name, intel_proc_dir);
		remove_proc_entry(name, intel_proc_dir);
	}

	/* If the intel dir is empty, remove it */

	for(proc_dir = intel_proc_dir->subdir; proc_dir;
		proc_dir = proc_dir->next) {

		/* ignore . and .. */

		if(*(proc_dir->name) == '.')
			continue;
		break;
	}

	if(!proc_dir)
		remove_proc_entry(ADAPTERS_PROC_DIR, proc_net);
}


static int __devinit
e1000_proc_singles_create(struct proc_dir_entry *parent,
                          struct list_head *proc_list_head)
{
	struct list_head *curr;
	struct proc_list *elem;

	list_for_each(curr, proc_list_head) {
		struct proc_dir_entry *proc_entry;

		elem = list_entry(curr, struct proc_list, list);

		if(!strlen(elem->tag))
			continue;

		if(!(proc_entry =
			create_proc_entry(elem->tag, S_IFREG, parent)))
			return 0;

		proc_entry->read_proc = e1000_proc_single_read;
		proc_entry->data = elem;
		SET_MODULE_OWNER(proc_entry);
	}

	return 1;
}

static void __devinit
e1000_proc_dirs_create(void *data, char *name, 
                       struct list_head *proc_list_head)
{
	struct proc_dir_entry *intel_proc_dir, *proc_dir, *info_entry;
	char info_name[strlen(name) + strlen(".info")];

	for(intel_proc_dir = proc_net->subdir; intel_proc_dir;
		intel_proc_dir = intel_proc_dir->next) {
		if((intel_proc_dir->namelen == strlen(ADAPTERS_PROC_DIR)) &&
		   !memcmp(intel_proc_dir->name, ADAPTERS_PROC_DIR, strlen(ADAPTERS_PROC_DIR)))
			break;
	}

	if(!intel_proc_dir)
		if(!(intel_proc_dir =
			create_proc_entry(ADAPTERS_PROC_DIR,
				S_IFDIR, proc_net)))
			return;

	if(!(proc_dir =
		create_proc_entry(name, S_IFDIR, intel_proc_dir)))
		return;
	SET_MODULE_OWNER(proc_dir);

	if(!e1000_proc_singles_create(proc_dir, proc_list_head))
		return;

	strcpy(info_name, name);
	strcat(info_name, ".info");

	if(!(info_entry =
		create_proc_entry(info_name, S_IFREG, intel_proc_dir)))
		return;
	SET_MODULE_OWNER(info_entry);

	info_entry->read_proc = e1000_proc_info_read;
	info_entry->data = proc_list_head;
}

static void __devinit
e1000_proc_list_add(struct list_head *proc_list_head, char *tag,
                    void *data, size_t len, 
		    char *(*func)(void *, size_t, char *))
{
	struct proc_list *new = (struct proc_list *)
		kmalloc(sizeof(struct proc_list), GFP_KERNEL);

	if(!new)
		return;

	strncpy(new->tag, tag, TAG_MAX_LENGTH);
	new->data = data;
	new->len  = len;
	new->func = func;

	list_add_tail(&new->list, proc_list_head);
}

static void __devexit
e1000_proc_list_free(struct list_head *proc_list_head)
{
	struct proc_list *elem;

	while(!list_empty(proc_list_head)) {
		elem = list_entry(proc_list_head->next, struct proc_list, list);
		list_del(&elem->list);
		kfree(elem);
	}
}

/*
 * General purpose formating functions
 */

static char *
e1000_proc_str(void *data, size_t len, char *buf)
{
	sprintf(buf, "%s", (char *)data);
	return buf;
}

static char *
e1000_proc_hex(void *data, size_t len, char *buf)
{
	switch(len) {
	case sizeof(uint8_t):
		sprintf(buf, "0x%02x", *(uint8_t *)data);
		break;
	case sizeof(uint16_t):
		sprintf(buf, "0x%04x", *(uint16_t *)data);
		break;
	case sizeof(uint32_t):
		sprintf(buf, "0x%08x", *(uint32_t *)data);
		break;
	case sizeof(uint64_t):
		sprintf(buf, "0x%08Lx", (unsigned long long)*(uint64_t *)data);
		break;
	}
	return buf;
}

static char *
e1000_proc_unsigned(void *data, size_t len, char *buf)
{
	switch(len) {
	case sizeof(uint8_t):
		sprintf(buf, "%u", *(uint8_t *)data);
		break;
	case sizeof(uint16_t):
		sprintf(buf, "%u", *(uint16_t *)data);
		break;
	case sizeof(uint32_t):
		sprintf(buf, "%u", *(uint32_t *)data);
		break;
	case sizeof(uint64_t):
		sprintf(buf, "%Lu", (unsigned long long)*(uint64_t *)data);
		break;
	}
	return buf;
}

/*
 * Specific formating functions
 */

static char *
e1000_proc_part_number(void *data, size_t len, char *buf)
{
	sprintf(buf, "%06x-%03x", *(uint32_t *)data >> 8,
	        *(uint32_t *)data & 0x000000FF);
	return buf;
}

static char *
e1000_proc_slot(void *data, size_t len, char *buf)
{
	struct e1000_adapter *adapter = data;
	sprintf(buf, "%u", PCI_SLOT(adapter->pdev->devfn));
	return buf;
}

static char *
e1000_proc_bus_type(void *data, size_t len, char *buf)
{
	e1000_bus_type bus_type = *(e1000_bus_type *)data;
	sprintf(buf,
		bus_type == e1000_bus_type_pci  ? "PCI"   :
		bus_type == e1000_bus_type_pcix ? "PCI-X" :
		"UNKNOWN");
	return buf;
}

static char *
e1000_proc_bus_speed(void *data, size_t len, char *buf)
{
	e1000_bus_speed bus_speed = *(e1000_bus_speed *)data;
	sprintf(buf,
		bus_speed == e1000_bus_speed_33  ? "33MHz"  :
		bus_speed == e1000_bus_speed_66  ? "66MHz"  :
		bus_speed == e1000_bus_speed_100 ? "100MHz" :
		bus_speed == e1000_bus_speed_133 ? "133MHz" :
		"UNKNOWN");
	return buf;
}

static char *
e1000_proc_bus_width(void *data, size_t len, char *buf)
{
	e1000_bus_width bus_width = *(e1000_bus_width *)data;
	sprintf(buf,
		bus_width == e1000_bus_width_32 ? "32-bit"   :
		bus_width == e1000_bus_width_64 ? "64-bit" :
		"UNKNOWN");
	return buf;
}

static char *
e1000_proc_hwaddr(void *data, size_t len, char *buf)
{
	unsigned char *hwaddr = data;
	sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
		hwaddr[0], hwaddr[1], hwaddr[2],
		hwaddr[3], hwaddr[4], hwaddr[5]);
	return buf;
}

static char *
e1000_proc_link(void *data, size_t len, char *buf)
{
	struct e1000_adapter *adapter = data;
	sprintf(buf, netif_running(adapter->netdev) ?
		netif_carrier_ok(adapter->netdev) ?
		"up" : "down" : "N/A");
	return buf;
}

static char *
e1000_proc_link_speed(void *data, size_t len, char *buf)
{
	uint16_t link_speed = *(uint16_t *)data;
	sprintf(buf, link_speed ? "%u" : "N/A", link_speed);
	return buf;
}

static char *
e1000_proc_link_duplex(void *data, size_t len, char *buf)
{
	uint16_t link_duplex = *(uint16_t *)data;
	sprintf(buf,
		link_duplex == FULL_DUPLEX ? "Full" :
		link_duplex == HALF_DUPLEX ? "Half" :
		"N/A");
	return buf;
}

static char *
e1000_proc_state(void *data, size_t len, char *buf)
{
	struct e1000_adapter *adapter = data;
	sprintf(buf, adapter->netdev->flags & IFF_UP ? "up" : "down");
	return buf;
}

static char *
e1000_proc_media_type(void *data, size_t len, char *buf)
{
	struct e1000_adapter *adapter = data;
	sprintf(buf,
		adapter->hw.media_type == e1000_media_type_copper ?
		"Copper" : "Fiber");
	return buf;
}

static char *
e1000_proc_cable_length(void *data, size_t len, char *buf)
{
	struct e1000_adapter *adapter = data;
	e1000_cable_length cable_length = adapter->phy_info.cable_length;
	sprintf(buf, "%s%s",
		cable_length == e1000_cable_length_50      ? "0-50"    :
		cable_length == e1000_cable_length_50_80   ? "50-80"   :
		cable_length == e1000_cable_length_80_110  ? "80-110"  :
		cable_length == e1000_cable_length_110_140 ? "110-140" :
		cable_length == e1000_cable_length_140     ? "> 140"   :
		"Unknown",
		cable_length != e1000_cable_length_undefined ?
		" Meters (+/- 20 Meters)" : "");
	return buf;
}

static char *
e1000_proc_extended(void *data, size_t len, char *buf)
{
	struct e1000_adapter *adapter = data;
	e1000_10bt_ext_dist_enable dist_enable =
		adapter->phy_info.extended_10bt_distance;
	sprintf(buf,
		dist_enable == e1000_10bt_ext_dist_enable_normal ? "Disabled" :
		dist_enable == e1000_10bt_ext_dist_enable_lower  ? "Enabled"  :
		"Unknown");
	return buf;
}

static char *
e1000_proc_cable_polarity(void *data, size_t len, char *buf)
{
	struct e1000_adapter *adapter = data;
	e1000_rev_polarity polarity = adapter->phy_info.cable_polarity;
	sprintf(buf,
		polarity == e1000_rev_polarity_normal   ? "Normal"   :
		polarity == e1000_rev_polarity_reversed ? "Reversed" :
		"Unknown");
	return buf;
}

static char *
e1000_proc_polarity_correction(void *data, size_t len, char *buf)
{
	struct e1000_adapter *adapter = data;
	e1000_polarity_reversal correction =
		adapter->phy_info.polarity_correction;
	sprintf(buf,
		correction == e1000_polarity_reversal_enabled  ? "Disabled" :
		correction == e1000_polarity_reversal_disabled ? "Enabled"  :
		"Unknown");
	return buf;
}

static char *
e1000_proc_mdi_x_enabled(void *data, size_t len, char *buf)
{
	struct e1000_adapter *adapter = data;
	e1000_auto_x_mode mdix_mode = adapter->phy_info.mdix_mode;
	sprintf(buf, 
		mdix_mode == e1000_auto_x_mode_manual_mdi  ? "MDI"   : 
		mdix_mode == e1000_auto_x_mode_manual_mdix ? "MDI-X" :
		"Unknown");
	return buf;
}

static char *
e1000_proc_rx_status(void *data, size_t len, char *buf)
{
	e1000_1000t_rx_status rx_status = *(e1000_1000t_rx_status *)data;
	sprintf(buf,
		rx_status == e1000_1000t_rx_status_not_ok ? "NOT_OK" :
		rx_status == e1000_1000t_rx_status_ok     ? "OK"     :
		"Unknown");
	return buf;
}

/*
 * e1000_proc_list_setup - build link list of proc praramters
 * @adapter: board private structure
 *
 * Order matters - ethx.info entries are ordered in the order links 
 * are added to list.
 */

#define LIST_ADD_F(T,D,F) \
	e1000_proc_list_add(proc_list_head, (T), (D), sizeof(*(D)), (F))
#define LIST_ADD_BLANK() LIST_ADD_F("", NULL, NULL)
#define LIST_ADD_S(T,D) LIST_ADD_F((T), (D), e1000_proc_str)
#define LIST_ADD_H(T,D) LIST_ADD_F((T), (D), e1000_proc_hex)
#define LIST_ADD_U(T,D) LIST_ADD_F((T), (D), e1000_proc_unsigned)

static void __devinit
e1000_proc_list_setup(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct list_head *proc_list_head = &adapter->proc_list_head;

	INIT_LIST_HEAD(proc_list_head);

	LIST_ADD_S("Description", adapter->id_string);
	LIST_ADD_F("Part_Number", &adapter->part_num, e1000_proc_part_number);
	LIST_ADD_S("Driver_Name", e1000_driver_name);
	LIST_ADD_S("Driver_Version", e1000_driver_version);
	LIST_ADD_H("PCI_Vendor", &hw->vendor_id);
	LIST_ADD_H("PCI_Device_ID", &hw->device_id);
	LIST_ADD_H("PCI_Subsystem_Vendor", &hw->subsystem_vendor_id);
	LIST_ADD_H("PCI_Subsystem_ID", &hw->subsystem_id);
	LIST_ADD_H("PCI_Revision_ID", &hw->revision_id);
	LIST_ADD_U("PCI_Bus", &adapter->pdev->bus->number);
	LIST_ADD_F("PCI_Slot", adapter, e1000_proc_slot);

	if(adapter->hw.mac_type >= e1000_82543) {
		LIST_ADD_F("PCI_Bus_Type",
		           &hw->bus_type, e1000_proc_bus_type);
		LIST_ADD_F("PCI_Bus_Speed",
		           &hw->bus_speed, e1000_proc_bus_speed);
		LIST_ADD_F("PCI_Bus_Width",
		           &hw->bus_width, e1000_proc_bus_width);
	}

	LIST_ADD_U("IRQ", &adapter->pdev->irq);
	LIST_ADD_S("System_Device_Name", adapter->netdev->name);
	LIST_ADD_F("Current_HWaddr",
	            adapter->netdev->dev_addr, e1000_proc_hwaddr);
	LIST_ADD_F("Permanent_HWaddr",
	            adapter->hw.perm_mac_addr, e1000_proc_hwaddr);

	LIST_ADD_BLANK();

	LIST_ADD_F("Link", adapter, e1000_proc_link);
	LIST_ADD_F("Speed", &adapter->link_speed, e1000_proc_link_speed);
	LIST_ADD_F("Duplex", &adapter->link_duplex, e1000_proc_link_duplex);
	LIST_ADD_F("State", adapter, e1000_proc_state);

	LIST_ADD_BLANK();

	/* Standard net device stats */
	LIST_ADD_U("Rx_Packets", &adapter->net_stats.rx_packets);
	LIST_ADD_U("Tx_Packets", &adapter->net_stats.tx_packets);
	LIST_ADD_U("Rx_Bytes", &adapter->net_stats.rx_bytes);
	LIST_ADD_U("Tx_Bytes", &adapter->net_stats.tx_bytes);
	LIST_ADD_U("Rx_Errors", &adapter->net_stats.rx_errors);
	LIST_ADD_U("Tx_Errors", &adapter->net_stats.tx_errors);
	LIST_ADD_U("Rx_Dropped", &adapter->net_stats.rx_dropped);
	LIST_ADD_U("Tx_Dropped", &adapter->net_stats.tx_dropped);

	LIST_ADD_U("Multicast", &adapter->net_stats.multicast);
	LIST_ADD_U("Collisions", &adapter->net_stats.collisions);

	LIST_ADD_U("Rx_Length_Errors", &adapter->net_stats.rx_length_errors);
	LIST_ADD_U("Rx_Over_Errors", &adapter->net_stats.rx_over_errors);
	LIST_ADD_U("Rx_CRC_Errors", &adapter->net_stats.rx_crc_errors);
	LIST_ADD_U("Rx_Frame_Errors", &adapter->net_stats.rx_frame_errors);
	LIST_ADD_U("Rx_FIFO_Errors", &adapter->net_stats.rx_fifo_errors);
	LIST_ADD_U("Rx_Missed_Errors", &adapter->net_stats.rx_missed_errors);

	LIST_ADD_U("Tx_Aborted_Errors", &adapter->net_stats.tx_aborted_errors);
	LIST_ADD_U("Tx_Carrier_Errors", &adapter->net_stats.tx_carrier_errors);
	LIST_ADD_U("Tx_FIFO_Errors", &adapter->net_stats.tx_fifo_errors);
	LIST_ADD_U("Tx_Heartbeat_Errors", 
	           &adapter->net_stats.tx_heartbeat_errors);
	LIST_ADD_U("Tx_Window_Errors", &adapter->net_stats.tx_window_errors);

	/* 8254x-specific stats */
	LIST_ADD_U("Tx_Abort_Late_Coll", &adapter->stats.latecol);
	LIST_ADD_U("Tx_Deferred_Ok", &adapter->stats.dc);
	LIST_ADD_U("Tx_Single_Coll_Ok", &adapter->stats.scc);
	LIST_ADD_U("Tx_Multi_Coll_Ok", &adapter->stats.mcc);
	LIST_ADD_U("Rx_Long_Length_Errors", &adapter->stats.roc);
	LIST_ADD_U("Rx_Short_Length_Errors", &adapter->stats.ruc);
	
	/* The 82542 does not have an alignment error count register */
	if(adapter->hw.mac_type >= e1000_82543)
		LIST_ADD_U("Rx_Align_Errors", &adapter->stats.algnerrc);
	
	LIST_ADD_U("Rx_Flow_Control_XON", &adapter->stats.xonrxc);
	LIST_ADD_U("Rx_Flow_Control_XOFF", &adapter->stats.xoffrxc);
	LIST_ADD_U("Tx_Flow_Control_XON", &adapter->stats.xontxc);
	LIST_ADD_U("Tx_Flow_Control_XOFF", &adapter->stats.xofftxc);
	LIST_ADD_U("Rx_CSum_Offload_Good", &adapter->hw_csum_good);
	LIST_ADD_U("Rx_CSum_Offload_Errors", &adapter->hw_csum_err);

	LIST_ADD_BLANK();

	/* Cable diags */
	LIST_ADD_F("PHY_Media_Type", adapter, e1000_proc_media_type);
	if(adapter->hw.media_type == e1000_media_type_copper) {
		LIST_ADD_F("PHY_Cable_Length",
		           adapter, e1000_proc_cable_length);
		LIST_ADD_F("PHY_Extended_10Base_T_Distance",
		           adapter, e1000_proc_extended);
		LIST_ADD_F("PHY_Cable_Polarity",
		           adapter, e1000_proc_cable_polarity);
		LIST_ADD_F("PHY_Disable_Polarity_Correction",
		           adapter, e1000_proc_polarity_correction);
		LIST_ADD_U("PHY_Idle_Errors", 
		           &adapter->phy_stats.idle_errors);
		LIST_ADD_U("PHY_Receive_Errors",
		           &adapter->phy_stats.receive_errors);
		LIST_ADD_F("PHY_MDI_X_Enabled",
		           adapter, e1000_proc_mdi_x_enabled);
		LIST_ADD_F("PHY_Local_Receiver_Status",
		           &adapter->phy_info.local_rx, 
			   e1000_proc_rx_status);
		LIST_ADD_F("PHY_Remote_Receiver_Status",
		           &adapter->phy_info.remote_rx, 
			   e1000_proc_rx_status);
	}

#ifdef E1000_COUNT_ICR
	LIST_ADD_U("Tx_D_Write_Back_IRQ", &adapter->icr_txdw);
	LIST_ADD_U("Tx_Q_Empty_IRQ", &adapter->icr_txqe);
	LIST_ADD_U("Link_Status_IRQ", &adapter->icr_lsc);
	LIST_ADD_U("Rx_Sequence_IRQ", &adapter->icr_rxseq);
	LIST_ADD_U("Rx_Threshold_IRQ", &adapter->icr_rxdmt);
	LIST_ADD_U("Rx_Overrun_IRQ", &adapter->icr_rxo);
	LIST_ADD_U("Rx_Timer_IRQ", &adapter->icr_rxt);
	LIST_ADD_U("MIDO_Complete_IRQ", &adapter->icr_mdac);
	LIST_ADD_U("/C/_Ordered_Set_IRQ", &adapter->icr_rxcfg);
	LIST_ADD_U("General_Purpose_IRQ", &adapter->icr_gpi);
#endif
}

/*
 * e1000_proc_dev_setup - create proc fs nodes and link list
 * @adapter: board private structure
 */

void __devinit
e1000_proc_dev_setup(struct e1000_adapter *adapter)
{
	e1000_proc_list_setup(adapter);

	e1000_proc_dirs_create(adapter, 
	                       adapter->netdev->name,
	                       &adapter->proc_list_head);
}

/*
 * e1000_proc_dev_free - free proc fs nodes and link list
 * @adapter: board private structure
 */

void __devexit
e1000_proc_dev_free(struct e1000_adapter *adapter)
{
	e1000_proc_dirs_free(adapter->netdev->name, &adapter->proc_list_head);

	e1000_proc_list_free(&adapter->proc_list_head);
}

#else /* CONFIG_PROC_FS */

void __devinit e1000_proc_dev_setup(struct e1000_adapter *adapter) {}
void __devexit e1000_proc_dev_free(struct e1000_adapter *adapter) {}

#endif /* CONFIG_PROC_FS */

