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
* Environment:                                                        *
*                                                                     *
**********************************************************************/

/* these are OS independent structures and defines */
#ifndef _ANS_H
#define _ANS_H
#include "base_comm.h"


typedef enum  { BD_ANS_SUCCESS, BD_ANS_FAILURE } BD_ANS_STATUS;
typedef enum { BD_ANS_FALSE, BD_ANS_TRUE } BD_ANS_BOOLEAN;

#include "ans_os.h"
#include "ans_hw.h"


#define BD_ANS_LINK_STATUS_SUPPORTED    0x00000001
#define BD_ANS_SPEED_STATUS_SUPPORTED   0x00000002
#define BD_ANS_DUPLEX_STATUS_SUPPORTED  0x00000004
#define BD_ANS_HW_FAIL_STATUS_SUPPORTED 0x00000008
#define BD_ANS_SUSPEND_STATUS_SUPPORTED 0x00000010
#define BD_ANS_RESET_STATUS_SUPPORTED   0x00000020

/* communication status flags */
#define IANS_COMMUNICATION_DOWN	0
#define IANS_COMMUNICATION_UP 1
#define IANS_STATUS_REPORTING_OFF 0
#define IANS_STATUS_REPORTING_ON 1
#define IANS_VLAN_FILTERING_OFF 0
#define IANS_VLAN_FILTERING_ON 1
#define IANS_VLAN_MODE_OFF 0
#define IANS_VLAN_MODE_ON 1
#define IANS_ROUTING_ON     1
#define IANS_ROUTING_OFF    0

/* vlan related stuff */
#define QTAG_TYPE          0x8100 
#define VLAN_PRIORITY_MASK 0xE000
#define VLAN_TR_FLAG_MASK  0x1000
#define VLAN_ID_MASK       0x0FFF
#define QTAG_SIZE 4
#ifndef ETHERNET_ADDRESS_LENGTH
#define ETHERNET_ADDRESS_LENGTH	6
#endif
#define MAX_NUM_VLAN		128
#define INVALID_VLAN_ID		0xffff

typedef struct _x8021Q_tag_t {
    UINT16 EtherType;
    UINT16 VLAN_ID;
} x8021Q_tag_t, *p8021Q_tag_t;

/*- Ethernet over VLAN Header */
typedef struct _eth_vlan_header_t 
{
    UCHAR     eth_dest[ETHERNET_ADDRESS_LENGTH];
    UCHAR     eth_src[ETHERNET_ADDRESS_LENGTH];
    x8021Q_tag_t	Qtag;
    UINT16      eth_typelen;
} eth_vlan_header_t, *peth_vlan_header_t;

typedef struct _iANSsupport_t{
    /* base driver/ans comm status fields */
    UINT32 iANS_status;		/* communication to iANS UP/DOWN*/
    UINT32 vlan_mode;		/* VLan mode switch         */
    UINT32 vlan_filtering_mode;	/* VLan filtering on/off   */
    UINT32 num_vlan_filter;	/* number of vlans to filter */
    UINT32 tag_mode;		/* see IANS_BD_TAGGING_MODE */
    UINT32 reporting_mode;	/* status reporting switch  */
    UINT32 timer_id;		/* iANS watchdog timer ID */
    UINT32 attributed_mode;     /* sending TLVs with our packets */
    UINT32 routing_mode;        /* sending rx packets to ans proto. */
    
    /* general driver status fields */
    UINT32 *link_status;            
    UINT32 *line_speed;
    UINT32 *duplex;
    UINT32 *hw_fail;
    UINT32 *suspended;
    UINT32 *in_reset;
    IANS_BD_PARAM_STATUS prev_status; /* status struct to be compared with current */
	
    /* base driver capabilities */
    UINT32 status_support_flags;      /* flags to indicate which status is supported */
    UINT32 max_vlan_ID_supported;     /* max Vlan ID supported by base-driver*/
    UINT32 vlan_table_size;           /* size of VlanID filtering table */
    BD_ANS_BOOLEAN ISL_tag_support;   /* base driver supports ISL */
    BD_ANS_BOOLEAN IEEE_tag_support;  /* base driver supports 802.3ac */
    BD_ANS_BOOLEAN vlan_support;      /* base driver supports VLan */
    BD_ANS_BOOLEAN vlan_filtering_support; /* base driver supports Vlan filtering*/    
    BD_ANS_BOOLEAN can_set_mac_addr;  /* can the adapter change it's mac addr */
    BD_ANS_BOOLEAN supports_stop_promiscuous; 
    UINT32 bd_flags;
    BD_ANS_BOOLEAN vlan_offload_support;         
    UINT32 available_speeds;

    /* the vlan table */
    UINT16 VlanID[MAX_NUM_VLAN];
} iANSsupport_t, *piANSsupport_t;


#define ANS_BD_SUPPORTS(bool_val) \
    ((bool_val) == BD_ANS_TRUE)?IANS_BD_SUPPORTS:IANS_BD_DOES_NOT_SUPPORT;

#define GET_NEXT_TLV(tlv) \
	(Per_Frame_Attribute_Header *)((UINT8 *)(&(tlv->AttributeLength) + \
	sizeof(tlv->AttributeLength) + (tlv)->AttributeLength))

/* function prototypes */
extern VOID bd_ans_Init(iANSsupport_t *iANSdata);
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
    VOID *ans_buffer);
extern BD_ANS_STATUS bd_ans_FillStatus(BOARD_PRIVATE_STRUCT *bps,
    iANSsupport_t *iANSdata,
    VOID *ans_buffer);
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
    UINT32 *tlv_list_length);
extern BD_ANS_STATUS bd_ans_Transmit(BOARD_PRIVATE_STRUCT *bps,
    iANSsupport_t *iANSdata,
    pPer_Frame_Attribute_Header pTLV,
    HW_TX_DESCRIPTOR *txd,
    OS_DATA **frame,
    UINT16 *vlanid);
extern BD_ANS_BOOLEAN bd_ans_IsQtagPacket(BOARD_PRIVATE_STRUCT *bps,
    iANSsupport_t *iANSdata,
    HW_RX_DESCRIPTOR *rxd,
    peth_vlan_header_t header);
extern UINT16 bd_ans_GetVlanId(BOARD_PRIVATE_STRUCT *bps,
    iANSsupport_t *iANSdata,
    HW_RX_DESCRIPTOR *rxd,
    peth_vlan_header_t header);
extern UINT32 bd_ans_AttributeFill(iANS_Attribute_ID attr_id,
    void *pTLV,
    void *data);
extern UINT32 bd_ans_ExtractValue(Per_Frame_Attribute_Header *pTLV);


extern void (*ans_notify)(struct device *dev, int ind_type);

extern BD_ANS_STATUS bd_ans_SetCallback(BOARD_PRIVATE_STRUCT *bps,
    iANSsupport_t *iANSdata,
    IANS_BD_PARAM_HEADER *header);

#endif
