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

/* /proc definitions */
#define ADAPTERS_PROC_DIR      	"PRO_LAN_Adapters"

#define DESCRIPTION_TAG        	"Description"
#define DRVR_NAME_TAG	    	"Driver_Name"
#define DRVR_VERSION_TAG	"Driver_Version"
#define PCI_VENDOR_TAG		"PCI_Vendor"
#define PCI_DEVICE_ID_TAG		"PCI_Device_ID"
#define PCI_SUBSYSTEM_VENDOR_TAG	"PCI_Subsystem_Vendor"
#define PCI_SUBSYSTEM_ID_TAG	"PCI_Subsystem_ID"
#define PCI_REVISION_ID_TAG	"PCI_Revision_ID"
#define PCI_BUS_TAG		"PCI_Bus"
#define PCI_SLOT_TAG 		"PCI_Slot"
#define IRQ_TAG			"IRQ"
#define SYSTEM_DEVICE_NAME_TAG	"System_Device_Name"
#define CURRENT_HWADDR_TAG	"Current_HWaddr"
#define PERMANENT_HWADDR_TAG 	"Permanent_HWaddr"

#define LINK_TAG 		"Link"
#define SPEED_TAG		"Speed"
#define DUPLEX_TAG	 	"Duplex"
#define STATE_TAG		"State"

#define RX_PACKETS_TAG		"Rx_Packets"
#define TX_PACKETS_TAG		"Tx_Packets"
#define RX_BYTES_TAG		"Rx_Bytes"
#define TX_BYTES_TAG		"Tx_Bytes"
#define RX_ERRORS_TAG		"Rx_Errors"
#define TX_ERRORS_TAG		"Tx_Errors"
#define RX_DROPPED_TAG		"Rx_Dropped"
#define TX_DROPPED_TAG		"Tx_Dropped"
#define	MULTICAST_TAG		"Multicast"
#define COLLISIONS_TAG			"Collisions"
#define RX_LENGTH_ERRORS_TAG		"Rx_Length_Errors"
#define RX_OVER_ERRORS_TAG		"Rx_Over_Errors"
#define RX_CRC_ERRORS_TAG		"Rx_CRC_ERRORS"
#define RX_FRAME_ERRORS_TAG		"Rx_Frame_Errors"
#define RX_FIFO_ERRORS_TAG		"Rx_FIFO_Errors"
#define RX_MISSED_ERRORS_TAG		"Rx_Missed_Errors"
#define TX_ABORTED_ERRORS_TAG		"Tx_Aborted_Errors"
#define TX_CARRIER_ERRORS_TAG		"Tx_Carrier_Errors"
#define TX_FIFO_ERRORS_TAG		"Tx_FIFO_Errors"
#define TX_HEARTBEAT_ERRORS_TAG		"Tx_Heartbeat_Errors"
#define TX_WINDOW_ERRORS_TAG		"Tx_Window_Errors"

#define RX_TCP_CHECKSUM_GOOD_TAG	"Rx_TCP_Checksum_Good"
#define RX_TCP_CHECKSUM_BAD_TAG		"Rx_TCP_Checksum_Bad"
#define TX_TCP_CHECKSUM_GOOD_TAG	"Tx_TCP_Checksum_Good"
#define TX_TCP_CHECKSUM_BAD_TAG		"Tx_TCP_Checksum_Bad"

#define TX_LATE_COLL_TAG		"Tx_Abort_Late_Coll"
#define TX_DEFERRED_TAG			"Tx_Deferred_Ok"
#define TX_SINGLE_COLL_TAG		"Tx_Single_Coll_Ok"
#define TX_MULTI_COLL_TAG		"Tx_Multi_Coll_Ok"
#define RX_LONG_ERRORS_TAG		"Rx_Long_Length_Errors"
#define RX_SHORT_ERRORS_TAG		"Rx_Short_Length_Errors"
#define RX_ALIGN_ERRORS_TAG		"Rx_Align_Errors"
#define RX_XON_TAG				"Rx_Flow_Control_XON"
#define RX_XOFF_TAG				"Rx_Flow_Control_XOFF"
#define TX_XON_TAG				"Tx_Flow_Control_XON"
#define TX_XOFF_TAG				"Tx_Flow_Control_XOFF"

/* symbols exported to e1000_main */
extern struct proc_dir_entry *e1000_proc_dir;
extern int e1000_create_proc_dev(bd_config_t *bdp);
extern void e1000_remove_proc_dev(device_t *dev);

