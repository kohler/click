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

/* these are the hardware specific (not OS specific) routines needed by the 
** bd_ans module
*/
#define BD_ANS_HW_FLAGS(bps) IANS_BD_FLAG4
#define BD_ANS_HW_AVAILABLE_SPEEDS(bps) bd_ans_hw_available_speeds(BD_ANS_DRV_PHY_ID(bps))

/* function prototypes */
extern UINT32 bd_ans_hw_available_speeds(UINT32 phyID);
#ifdef IANS_BASE_VLAN_TAGGING
extern BD_ANS_BOOLEAN bd_ans_hw_IsQtagPacket(BOARD_PRIVATE_STRUCT *bps, HW_RX_DESCRIPTOR *rxd);
extern BD_ANS_STATUS bd_ans_hw_InsertQtagHW(BOARD_PRIVATE_STRUCT *bps, HW_TX_DESCRIPTOR *txd, UINT16 *vlanid);
extern UINT16 bd_ans_hw_GetVlanId(BOARD_PRIVATE_STRUCT *bps,
								  HW_RX_DESCRIPTOR *rxd);
extern BD_ANS_STATUS bd_ans_hw_EnableVLAN(BOARD_PRIVATE_STRUCT *Adapter);
extern BD_ANS_STATUS bd_ans_hw_DisableTagging(BOARD_PRIVATE_STRUCT *Adapter);
extern BD_ANS_STATUS bd_ans_hw_EnablePriorityRx(BOARD_PRIVATE_STRUCT *Adapter);
#endif
 


