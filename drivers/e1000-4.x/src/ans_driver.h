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

/* driver defines specific to the linux gigabit driver */
#ifndef _ANS_DRIVER_H
#define _ANS_DRIVER_H

/* hardware specfic defines */
#define BOARD_PRIVATE_STRUCT struct e1000_adapter
#define HW_RX_DESCRIPTOR struct e1000_rx_desc
#define HW_TX_DESCRIPTOR struct e1000_tx_desc
#define FRAME_DATA unsigned char
#include "e1000.h"

/* you must include this after you define above stuff */
#include "ans.h"

#define ANS_PRIVATE_DATA_FIELD(bps) ((bps)->iANSdata)
#define GIGABIT_ADAPTER_STRUCT(bps) (bps)
#define DRIVER_DEV_FIELD(bps) ((bps)->netdev)
#define BD_ANS_HW_FLAGS(bps) bd_ans_drv_hw_flags(bps)

#define BD_ANS_DRV_STATUS_SUPPORT_FLAGS (BD_ANS_LINK_STATUS_SUPPORTED | BD_ANS_SPEED_STATUS_SUPPORTED |BD_ANS_DUPLEX_STATUS_SUPPORTED |BD_ANS_SUSPEND_STATUS_SUPPORTED)
#define BD_ANS_DRV_MAX_VLAN_ID(bps) 4096 
#define BD_ANS_DRV_MAX_VLAN_TABLE_SIZE(bps) 4096
#define BD_ANS_DRV_IEEE_TAG_SUPPORT(bps) \
		(((bps)->hw.mac_type < e1000_82543) ? \
		BD_ANS_FALSE : BD_ANS_TRUE)
#define BD_ANS_DRV_VLAN_SUPPORT(bps) \
		(BD_ANS_DRV_IEEE_TAG_SUPPORT(bps))
#define BD_ANS_DRV_VLAN_FILTER_SUPPORT(bps) BD_ANS_TRUE
#define BD_ANS_DRV_VLAN_OFFLOAD_SUPPORT(bps) BD_ANS_TRUE
#ifndef MAX_ETHERNET_PACKET_SIZE
#define MAX_ETHERNET_PACKET_SIZE 1514
#endif

#define BD_ANS_DRV_PHY_ID(bps)    ((bps)->hw.phy_id)
#define BD_ANS_DRV_REV_ID(bps)    ((bps)->RevId)
#define BD_ANS_DRV_SUBSYS_ID(bps) ((bps)->SubSystemId)
#define WISEMAN_FIRST_REV	WISEMAN_2_0_REV_ID

#ifndef BYTE_SWAP_WORD
#define BYTE_SWAP_WORD(word) ((((word) & 0x00ff) << 8) \
								| (((word) & 0xff00) >> 8))
#endif
/* function prototypes */
extern void bd_ans_drv_InitANS(BOARD_PRIVATE_STRUCT *bps, iANSsupport_t *iANSdata);
extern void bd_ans_drv_UpdateStatus(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_ConfigureTagging(BOARD_PRIVATE_STRUCT *bdp);
extern BD_ANS_STATUS bd_ans_drv_ConfigureVlanTable(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_ConfigureVlan(BOARD_PRIVATE_STRUCT *bps);
extern void bd_ans_drv_StopWatchdog(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_StopPromiscuousMode(BOARD_PRIVATE_STRUCT *bps);
extern u32 bd_ans_drv_StartWatchdog(BOARD_PRIVATE_STRUCT *bps);
extern void bd_ans_drv_ReturnOSFrameDescriptors(BOARD_PRIVATE_STRUCT *bps);
extern void bd_ans_drv_SetupHWRxStructures(BOARD_PRIVATE_STRUCT *bps);
extern void bd_ans_drv_AllocateOSFrameDescriptors(BOARD_PRIVATE_STRUCT *bps);
extern u32 bd_ans_drv_hw_flags(BOARD_PRIVATE_STRUCT *bps);
#endif
