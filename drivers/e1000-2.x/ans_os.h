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
* Module Name:  ans_os.h                                              *
*                                                                     *
* Abstract: this file contains OS specific defines                    *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/


#ifndef _ANS_OS_H
#define _ANS_OS_H
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/skbuff.h>
#include <linux/netdevice.h>

/* define the types used for the bd_ans module that are
 * os specific 
 */
#ifndef UINT32
#define UINT32 uint32_t
#endif

#ifndef VOID
#define VOID void
#endif 

#ifndef UCHAR 
#define UCHAR  unsigned char
#endif

#ifndef UINT8
#define UINT8 uint8_t
#endif

#ifndef UINT16
#define UINT16 uint16_t
#endif
 
/* debug macros for this os */
#ifdef DEBUG
#define DEBUGLOG(s) printk(s);
#define DEBUGLOG1(s, arg) printk(s, arg);
#define DEBUGLOG2(s, arg1, arg2) printk(s, arg1, arg2);
#else
#define DEBUGLOG(s) 
#define DEBUGLOG1(s, arg) 
#define DEBUGLOG2(s, arg1, arg2) 
#endif


/* definition of ethernet frame */
#define OS_DATA struct sk_buff 

/* how we report line speed for this os */
#define BD_ANS_10_MBPS  10
#define BD_ANS_100_MBPS 100
#define BD_ANS_1000_MBPS 1000

/* how we report duplex for this os */
#define BD_ANS_DUPLEX_FULL 2
#define BD_ANS_DUPLEX_HALF 1

/* os specific capabilities */
#define BD_ANS_OS_STOP_PROM_SUPPORT BD_ANS_FALSE
#define BD_ANS_OS_MAC_ADDR_SUPPORT BD_ANS_TRUE
#define BD_ANS_OS_CAN_ROUTE_RX(bps)  BD_ANS_TRUE
#define BD_ANS_OS_CAN_ROUTE_EVENT(bps)   BD_ANS_FALSE

/* macro to calculate maximum space needed to reserve 
** at head of skb for ANS extra info.
*/
#ifdef IANS_BASE_VLAN_TAGGING
#define BD_ANS_INFO_SIZE (sizeof(IANS_ATTR_HEADER) + \
						  sizeof(VLAN_ID_Per_Frame_Info) + \
						  sizeof(Last_Attribute))
#else
#define BD_ANS_INFO_SIZE (sizeof(IANS_ATTR_HEADER) + \
                          sizeof(Last_Attribute))
#endif

/* function prototypes */
extern void bd_ans_os_ReserveSpaceForANS(struct sk_buff *skb);
extern UINT32 bd_ans_os_AttributeFill(iANS_Attribute_ID attr_id, 
    struct sk_buff *skb, 
    UINT32 prev_tlv_length,
    void *data);
extern BD_ANS_BOOLEAN bd_ans_os_AllocateTLV(struct sk_buff *frame, 
    struct sk_buff **tlv);

#ifdef IANS_BASE_VLAN_TAGGING                                            
extern void bd_ans_os_StripQtagSW(struct sk_buff *skb);
extern BD_ANS_STATUS bd_ans_os_InsertQtagSW(BOARD_PRIVATE_STRUCT *bps, 
    struct sk_buff **skb, 
    UINT16 *vlan_id);
#endif

extern void bd_ans_os_Watchdog(struct device *dev, 
    BOARD_PRIVATE_STRUCT *bps);
extern int bd_ans_os_Receive(BOARD_PRIVATE_STRUCT *bps,
    HW_RX_DESCRIPTOR *rxd,
    struct sk_buff *skb );
extern int bd_ans_os_Transmit(BOARD_PRIVATE_STRUCT *bps, 
    HW_TX_DESCRIPTOR *txd,
    struct sk_buff **skb );
extern int bd_ans_os_Ioctl(struct device *dev, 
    struct ifreq *ifr, 
    int cmd);                                      

#endif



