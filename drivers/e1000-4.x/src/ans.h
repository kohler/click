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

/**********************************************************************
*                                                                     *
* INTEL CORPORATION                                                   *
*                                                                     *
* This software is supplied under the terms of the license included   *
* above.  All use of this driver must be in accordance with the terms *
* of that license.                                                    *
*                                                                     *
* Module Name:  ans.h                                                 *
*                                                                     *
* Abstract:                                                           *
*                                                                     *
* Environment:  This file is intended to be shared among Linux and    *
*               Unixware operating systems.                           *
*                                                                     *
**********************************************************************/
#ifndef _ANS_H
#define _ANS_H

#include "ans_interface.h"

typedef enum  { BD_ANS_SUCCESS, BD_ANS_FAILURE } BD_ANS_STATUS;
typedef enum { BD_ANS_FALSE, BD_ANS_TRUE } BD_ANS_BOOLEAN;

/*#include "ans_hw.h"*/

#define BD_ANS_LINK_STATUS_SUPPORTED    0x00000001
#define BD_ANS_SPEED_STATUS_SUPPORTED   0x00000002
#define BD_ANS_DUPLEX_STATUS_SUPPORTED  0x00000004
#define BD_ANS_HW_FAIL_STATUS_SUPPORTED 0x00000008
#define BD_ANS_SUSPEND_STATUS_SUPPORTED 0x00000010
#define BD_ANS_RESET_STATUS_SUPPORTED   0x00000020

/* communication status flags */
#define IANS_COMMUNICATION_DOWN 0
#define IANS_COMMUNICATION_UP 1
#define IANS_STATUS_REPORTING_OFF 0
#define IANS_STATUS_REPORTING_ON 1
#define IANS_VLAN_FILTERING_OFF 0
#define IANS_VLAN_FILTERING_ON 1
#define IANS_VLAN_MODE_OFF 0
#define IANS_VLAN_MODE_ON 1
#define IANS_ROUTING_OFF    0
#define IANS_ROUTING_ON     1

/* vlan related stuff */
#define QTAG_TYPE          0x8100 
#define VLAN_PRIORITY_MASK 0xE000
#define VLAN_TR_FLAG_MASK  0x1000
#define VLAN_ID_MASK       0x0FFF
#define QTAG_SIZE 4
#ifndef ETHERNET_ADDRESS_LENGTH
#define ETHERNET_ADDRESS_LENGTH	6
#endif
#define MAX_NUM_VLAN            128
#define INVALID_VLAN_ID         0xffff

typedef struct _x8021Q_tag_t {
    u16 EtherType;
    u16 VLAN_ID;
} x8021Q_tag_t, *p8021Q_tag_t;

/*- Ethernet over VLAN Header */
typedef struct _eth_vlan_header_t 
{
    u8     eth_dest[ETHERNET_ADDRESS_LENGTH];
    u8     eth_src[ETHERNET_ADDRESS_LENGTH];
    x8021Q_tag_t        Qtag;
    u16      eth_typelen;
} eth_vlan_header_t, *peth_vlan_header_t;

typedef struct _iANSsupport_t{
    /* base driver/ans comm status fields */
    u32 iANS_status;         /* communication to iANS UP/DOWN*/
    u32 vlan_mode;           /* VLan mode switch         */
    u32 vlan_filtering_mode; /* VLan filtering on/off   */
    u32 num_vlan_filter;     /* number of vlans to filter */
    u32 tag_mode;            /* see IANS_BD_TAGGING_MODE */
    u32 reporting_mode;      /* status reporting switch  */
    u32 timer_id;            /* iANS watchdog timer ID */
    u32 attributed_mode;     /* sending TLVs with our packets */
    u32 routing_mode;        /* sending rx packets to ans proto. */
    
    /* general driver status fields */
    u32 *link_status;            
    u32 *line_speed;
    u32 *duplex;
    u32 *hw_fail;
    u32 *suspended;
    u32 *in_reset;
    IANS_BD_PARAM_STATUS prev_status; /* status struct to be compared with current */
  
    /* base driver capabilities */
    u32 status_support_flags;     /* flags to indicate which status is supported */
    u32 max_vlan_ID_supported;    /* max Vlan ID supported by base-driver*/
    u32 vlan_table_size;          /* size of VlanID filtering table */
    BD_ANS_BOOLEAN IEEE_tag_support; /* base driver supports 802.3ac */
    BD_ANS_BOOLEAN vlan_support;     /* base driver supports VLan */
    BD_ANS_BOOLEAN vlan_filtering_support; /* base driver supports Vlan filtering*/    
    BD_ANS_BOOLEAN can_set_mac_addr; /* can the adapter change it's mac addr */
    BD_ANS_BOOLEAN supports_stop_promiscuous; 
    union {
	u32 is_server_adapter;
	u32 bd_flags;
    } flags;
    BD_ANS_BOOLEAN vlan_offload_support;         
    u32 available_speeds;
  
    /* the vlan table */
    u16 VlanID[MAX_NUM_VLAN];
} iANSsupport_t, *piANSsupport_t;

#include "ans_os.h"
#include "ans_hw.h"

#define ANS_BD_SUPPORTS(bool_val) \
    ((bool_val) == BD_ANS_TRUE)?IANS_BD_SUPPORTS:IANS_BD_DOES_NOT_SUPPORT;

#define GET_NEXT_TLV(tlv) \
    (Per_Frame_Attribute_Header *)((u8 *)(&(tlv->AttributeLength)) + \
        sizeof(tlv->AttributeLength) + (tlv)->AttributeLength)

/* function prototypes */
extern void bd_ans_Init(iANSsupport_t *iANSdata);
extern BD_ANS_STATUS bd_ans_Identify(BOARD_PRIVATE_STRUCT *bps,
                                     iANSsupport_t *iANSdata,
                                     IANS_BD_PARAM_HEADER *header);      
extern BD_ANS_STATUS bd_ans_Disconnect(BOARD_PRIVATE_STRUCT *bps,
                                       iANSsupport_t *iANSdata,
                                       IANS_BD_PARAM_HEADER *header);
extern BD_ANS_STATUS bd_ans_ExtendedGetCapability(BOARD_PRIVATE_STRUCT *bps,
                                                  iANSsupport_t *iANSdata,
                                                  IANS_BD_PARAM_HEADER *header); 
extern BD_ANS_STATUS bd_ans_ExtendedSetMode(BOARD_PRIVATE_STRUCT *bps,
                                            iANSsupport_t *iANSdata,
                                            IANS_BD_PARAM_HEADER *header);   
extern BD_ANS_STATUS bd_ans_ExtendedStopPromiscuousMode(BOARD_PRIVATE_STRUCT *bps,
                                                        iANSsupport_t *iANSdata);
extern BD_ANS_STATUS bd_ans_ExtendedGetStatus(BOARD_PRIVATE_STRUCT *bps,
                                              iANSsupport_t *iANSdata,
                                              IANS_BD_PARAM_HEADER *header);
#ifdef IANS_BASE_VLAN_TAGGING
extern BD_ANS_STATUS bd_ans_TagGetCapability(BOARD_PRIVATE_STRUCT *bps,
                                             iANSsupport_t *iANSdata,
                                             IANS_BD_PARAM_HEADER *header);
extern BD_ANS_STATUS bd_ans_TagSetMode(BOARD_PRIVATE_STRUCT *bps,
                                       iANSsupport_t *iANSdata,
                                       IANS_BD_PARAM_HEADER *header);
#endif
#ifdef IANS_BASE_VLAN_ID
extern BD_ANS_STATUS bd_ans_VlanGetCapability(BOARD_PRIVATE_STRUCT *bps,
                                              iANSsupport_t *iANSdata,
                                              IANS_BD_PARAM_HEADER *header);
extern BD_ANS_STATUS bd_ans_VlanSetMode(BOARD_PRIVATE_STRUCT *bps,
                                        iANSsupport_t *iANSdata,
                                        IANS_BD_PARAM_HEADER *header);
extern BD_ANS_STATUS bd_ans_VlanSetTable(BOARD_PRIVATE_STRUCT *bps,
                                         iANSsupport_t *iANSdata,
                                         IANS_BD_PARAM_HEADER *header);
#endif
extern BD_ANS_STATUS bd_ans_ActivateFastPolling(BOARD_PRIVATE_STRUCT *bps,
                                                iANSsupport_t *iANSdata);
extern BD_ANS_STATUS bd_ans_DeActivateFastPolling(BOARD_PRIVATE_STRUCT *bps,
                                                  iANSsupport_t *iANSdata);
extern BD_ANS_STATUS bd_ans_SetReportingMode(BOARD_PRIVATE_STRUCT *bps,
                                             iANSsupport_t *iANSdata,
                                             void *ans_buffer);
extern BD_ANS_STATUS bd_ans_FillStatus(BOARD_PRIVATE_STRUCT *bps,
                                       iANSsupport_t *iANSdata,
                                       void *ans_buffer);
extern BD_ANS_STATUS bd_ans_ResetAllModes(BOARD_PRIVATE_STRUCT *bps,
                                          iANSsupport_t *iANSdata);
extern BD_ANS_STATUS bd_ans_GetAllCapabilities(BOARD_PRIVATE_STRUCT *bps,
                                               iANSsupport_t *iANSdata);
extern BD_ANS_STATUS bd_ans_Receive(BOARD_PRIVATE_STRUCT *bps,
                                    iANSsupport_t *iANSdata,
                                    HW_RX_DESCRIPTOR *rxd,
                                    FRAME_DATA *frame,
                                    OS_DATA *os_frame_data,
                                    OS_DATA **os_tlv,
                                    u32 *tlv_list_length);
extern BD_ANS_STATUS bd_ans_Transmit(BOARD_PRIVATE_STRUCT *bps,
                                     iANSsupport_t *iANSdata,
                                     pPer_Frame_Attribute_Header pTLV,
                                     HW_TX_DESCRIPTOR *txd,
                                     OS_DATA **frame,
                                     u16 *vlanid);
extern BD_ANS_BOOLEAN bd_ans_IsQtagPacket(BOARD_PRIVATE_STRUCT *bps,
                                          iANSsupport_t *iANSdata,
                                          HW_RX_DESCRIPTOR *rxd,
                                          peth_vlan_header_t header);
extern u16 bd_ans_GetVlanId(BOARD_PRIVATE_STRUCT *bps,
                               iANSsupport_t *iANSdata,
                               HW_RX_DESCRIPTOR *rxd,
                               peth_vlan_header_t header);
extern u32 bd_ans_AttributeFill(iANS_Attribute_ID attr_id,
                                   void *pTLV,
                                   void *data);
extern u32 bd_ans_ExtractValue(Per_Frame_Attribute_Header *pTLV);


extern BD_ANS_STATUS bd_ans_ProcessRequest(BOARD_PRIVATE_STRUCT *bps, 
                                    iANSsupport_t *iANSdata,
                                    IANS_BD_PARAM_HEADER *header);

extern void BD_ANS_BCOPY(u8 *destination, u8 *source, u32 length);
extern BD_ANS_BOOLEAN BD_ANS_BCMP(u8 *s1, u8 *s2, u32 length);

#endif
