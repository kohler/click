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

#include <linux/version.h>

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
 

/* In 2.3.14 the device structure was renamed to net_device */
#ifndef _DEVICE_T
#define _DEVICE_T
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,14) )
typedef struct device device_t;
#else
typedef struct net_device device_t;
#endif
#endif

/* debug macros for this os */
#ifdef DEBUG_ANS
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

#include <linux/module.h>
#define BD_ANS_DRV_LOCK   MOD_INC_USE_COUNT
#define BD_ANS_DRV_UNLOCK MOD_DEC_USE_COUNT

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

extern void bd_ans_os_Watchdog(device_t *dev, 
                               BOARD_PRIVATE_STRUCT *bps);
extern int bd_ans_os_Receive(BOARD_PRIVATE_STRUCT *bps,
                             HW_RX_DESCRIPTOR *rxd,
                             struct sk_buff *skb );
extern int bd_ans_os_Transmit(BOARD_PRIVATE_STRUCT *bps, 
                              HW_TX_DESCRIPTOR *txd,
                              struct sk_buff **skb );
extern int bd_ans_os_Ioctl(device_t *dev, 
                           struct ifreq *ifr, 
                           int cmd);                                      

extern void (*ans_notify)(device_t *dev, int ind_type);

extern BD_ANS_STATUS bd_ans_os_SetCallback(BOARD_PRIVATE_STRUCT *bps,
                                           IANS_BD_PARAM_HEADER *header);

extern BD_ANS_STATUS bd_ans_os_ExtendedSetMode(BOARD_PRIVATE_STRUCT *bps,
                                               iANSsupport_t *iANSdata,
                                               IANS_BD_PARAM_HEADER *header);   
extern BD_ANS_STATUS bd_ans_os_ExtendedGetCapability(BOARD_PRIVATE_STRUCT *bps,
                                                     iANSsupport_t *iANSdata,
                                                     IANS_BD_PARAM_HEADER *header);
extern BD_ANS_STATUS bd_ans_os_ProcessRequest(BOARD_PRIVATE_STRUCT *bps, 
                                              iANSsupport_t *iANSdata,
                                              IANS_BD_PARAM_HEADER *header);
extern BD_ANS_STATUS bd_ans_os_ActivateFastPolling(BOARD_PRIVATE_STRUCT *bps,      
                                                   iANSsupport_t *iANSdata);

extern BD_ANS_STATUS bd_ans_os_GetAllCapabilities(BOARD_PRIVATE_STRUCT *bps,
                                                  iANSsupport_t *iANSdata);
#endif



