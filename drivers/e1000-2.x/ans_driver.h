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

/* driver defines specific to the linux gigabit driver */
#ifndef _ANS_DRIVER_H
#define _ANS_DRIVER_H

// This is UGLY, need to resolve include dependancy problems
#define BOARD_PRIVATE_STRUCT void

/* hardware specfic defines */
#include "e1000_fxhw.h"
#define HW_RX_DESCRIPTOR E1000_RECEIVE_DESCRIPTOR
#define HW_TX_DESCRIPTOR E1000_TRANSMIT_DESCRIPTOR
#define FRAME_DATA unsigned char

/* you must include this after you define above stuff */
#include <ans.h>

// UGLY, caused by previous UGLY
#define ANS_PRIVATE_DATA_FIELD(bps) (((bd_config_t *)(bps))->iANSdata)
#define GIGABIT_ADAPTER_STRUCT(bps) (((bd_config_t *)(bps))->bddp)
#define DRIVER_DEV_FIELD(bps) (((bd_config_t *)(bps))->device)

#define BD_ANS_DRV_STATUS_SUPPORT_FLAGS (BD_ANS_LINK_STATUS_SUPPORTED | BD_ANS_SPEED_STATUS_SUPPORTED |BD_ANS_DUPLEX_STATUS_SUPPORTED |BD_ANS_SUSPEND_STATUS_SUPPORTED)
#define BD_ANS_DRV_MAX_VLAN_ID(bps) 4096 
#define BD_ANS_DRV_MAX_VLAN_TABLE_SIZE(bps) 4096
#define BD_ANS_DRV_ISL_TAG_SUPPORT(bps) BD_ANS_FALSE
#define BD_ANS_DRV_IEEE_TAG_SUPPORT(bps) \
		((((bd_config_t *)(bps))->bddp->MacType < MAC_LIVENGOOD) ? \
		BD_ANS_FALSE : BD_ANS_TRUE)
#define BD_ANS_DRV_VLAN_SUPPORT(bps) \
		(BD_ANS_DRV_IEEE_TAG_SUPPORT(bps) | BD_ANS_DRV_ISL_TAG_SUPPORT(bps))
#define BD_ANS_DRV_VLAN_FILTER_SUPPORT(bps) BD_ANS_TRUE
#define BD_ANS_DRV_VLAN_OFFLOAD_SUPPORT(bps) BD_ANS_TRUE
#ifndef MAX_ETHERNET_PACKET_SIZE
#define MAX_ETHERNET_PACKET_SIZE 1514
#endif

#define BD_ANS_DRV_PHY_ID(bps)    (((bd_config_t *)(bps))->bddp->PhyId)
#define BD_ANS_DRV_REV_ID(bps)    (((bd_config_t *)(bps))->bddp->RevId)
#define BD_ANS_DRV_SUBSYS_ID(bps) (((bd_config_t *)(bps))->bddp->SubSystemId)
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
extern VOID bd_ans_drv_StopWatchdog(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_StopPromiscuousMode(BOARD_PRIVATE_STRUCT *bps);
extern UINT32 bd_ans_drv_StartWatchdog(BOARD_PRIVATE_STRUCT *bps);
extern void bd_ans_drv_ReturnOSFrameDescriptors(BOARD_PRIVATE_STRUCT *bps);
extern void bd_ans_drv_SetupHWRxStructures(BOARD_PRIVATE_STRUCT *bps);
extern void bd_ans_drv_AllocateOSFrameDescriptors(BOARD_PRIVATE_STRUCT *bps);

#include <e1000.h>

#endif
