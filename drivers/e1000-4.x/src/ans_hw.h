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

/* these are the hardware specific (not OS specific) routines needed by the 
** bd_ans module
*/

#define BD_ANS_HW_AVAILABLE_SPEEDS(bps) bd_ans_hw_available_speeds(BD_ANS_DRV_PHY_ID(bps))

/* function prototypes */
extern u32 bd_ans_hw_available_speeds(u32 phyID);
#ifdef IANS_BASE_VLAN_TAGGING
extern BD_ANS_BOOLEAN bd_ans_hw_IsQtagPacket(BOARD_PRIVATE_STRUCT *bps, HW_RX_DESCRIPTOR *rxd);
extern BD_ANS_STATUS bd_ans_hw_InsertQtagHW(BOARD_PRIVATE_STRUCT *bps, HW_TX_DESCRIPTOR *txd, u16 *vlanid);
extern u16 bd_ans_hw_GetVlanId(BOARD_PRIVATE_STRUCT *bps,
                                                                  HW_RX_DESCRIPTOR *rxd);
extern BD_ANS_STATUS bd_ans_hw_EnableVLAN(BOARD_PRIVATE_STRUCT *Adapter);
extern BD_ANS_STATUS bd_ans_hw_DisableTagging(BOARD_PRIVATE_STRUCT *Adapter);
extern BD_ANS_STATUS bd_ans_hw_EnablePriorityRx(BOARD_PRIVATE_STRUCT *Adapter);
#endif
 


