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

/* hardware specific routines for the gigabit adapter */

#include "ans_driver.h"


/* bd_ans_hw_available_speeds()
**
**  This function will determine the speed capabilities of this adapter
**  based on the phy type.
**
**  Arguments:  u32 phy - the phy id of the adapter
**
**  Returns:    u32     - the available speeds of the driver.
*/
u32
bd_ans_hw_available_speeds(u32 phy)
{
    u32 speeds;
    
    DEBUGLOG("bd_ans_hw_available_speeds: enter\n");
    
    /* all gig's support 1000 at least */
    speeds = IANS_STATUS_LINK_SPEED_1000MBPS;
    
    /* check to see if we support 10/100 mbps */
    if (phy > 0) {
       speeds |= IANS_STATUS_LINK_SPEED_100MBPS;
       speeds |= IANS_STATUS_LINK_SPEED_10MBPS;
    }
    
    return (speeds);
}          


#ifdef IANS_BASE_VLAN_TAGGING
/* bd_ans_hw_IsQtagPacket
**  
**  This function will check the receive descriptor to see if we 
**  have received and 802.1q tagged packet
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private
**                                          data structure 
**              HW_RX_DESCRIPTOR *rxd - the receive descriptor
**
**  Returns:    BD_ANS_BOOLEAN - BD_ANS_TRUE if it is a qtagged packet
**                               BD_ANS_FALSE otherwise
*/
BD_ANS_BOOLEAN
bd_ans_hw_IsQtagPacket(BOARD_PRIVATE_STRUCT *bps, HW_RX_DESCRIPTOR *rxd)
{
    DEBUGLOG("bd_ans_hw_IsQtagPacket: enter\n");
    /* since gigabit is already cool and shares hardware structures
     * between OS's, we know we can use this field name without
     * using a macro.
     */
    return ((rxd->status & E1000_RXD_STAT_VP)?BD_ANS_TRUE:BD_ANS_FALSE);
}


/* bd_ans_hw_InsertQtagHW()
**
**  This function will insert a 802.1q tag into the correct field in the
**  transmit descriptor.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data structure
**              HW_TX_DESCRIPTOR *txd     - the adapter's transmit descriptor
**              UNIT16 *vlanid            - pointer to the vlan id to insert
**
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS always at this point.
*/
BD_ANS_STATUS
bd_ans_hw_InsertQtagHW(BOARD_PRIVATE_STRUCT *bps, HW_TX_DESCRIPTOR *txd, u16
*vlanid)
{
    iANSsupport_t *iANSdata;

    iANSdata = ANS_PRIVATE_DATA_FIELD(bps);

    DEBUGLOG("bd_ans_hw_InsertQtagHW: enter\n");

    /* tell hardware to add tag */
    txd->lower.data |= E1000_TXD_CMD_VLE;

    /* set the vlan id */
    txd->upper.fields.special = *vlanid;

    return BD_ANS_SUCCESS;
}



/* bd_ans_hw_GetVlanId()
**
**  This function will retrieve the vlan id from the receive descriptor
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data structure
**              HW_RX_DESCRIPTOR *rxd     - the adapter's receive descriptor
**
**  Returns:    u16 - the vlan id (masked off priority)
*/
u16
bd_ans_hw_GetVlanId(BOARD_PRIVATE_STRUCT *bps,
                                HW_RX_DESCRIPTOR *rxd)
{
    u16 VlanId;
    struct e1000_adapter *adapter = GIGABIT_ADAPTER_STRUCT(bps);
    
    DEBUGLOG("bd_ans_hw_GetVlanId: enter\n");
    
    // The packet has a tag, so extract it.
    // the first rev had the special field byte swapped
    if (adapter->hw.mac_type < e1000_82543)
    {
                DEBUGLOG("bd_ans_hw_GetVlanId: getting vlanid on WiseMan\n");
        VlanId = (BYTE_SWAP_WORD(rxd->special) & 
                 E1000_RXD_SPC_VLAN_MASK);
    }
    else
    {
        VlanId = (rxd->special & 
                 E1000_RXD_SPC_VLAN_MASK);
    }
        DEBUGLOG1("bd_ans_hw_GetVlanId: found vlan id %d\n", VlanId); 
    return VlanId;
}     
   

//*********************************************************************
// Name:         bd_ans_hw_EnableVLAN
//
// Description:  Enables IEEE VLAN tagging on the adapter.  Turns on 
//               VLAN filtering and tag stripping on receive, and enables
//               tagging on send.
//               This routine is based on Pat Connor's NDIS code.
//
// Author:       Mitch Williams
//
// Born on Date: 4/13/1999
//
// Arguments:    adapter - Pointer to HSM's adapter Data Space
//
// Returns:      ODISTAT    
//
// Modification log:
// Date       Who      Description
// --------   ---      ------------------------------------------------
// 3/21/00  kcarlson    Modified for generic ans_hw.c file to share amongst
//                      all OS who support ANS.
//*********************************************************************
BD_ANS_STATUS
bd_ans_hw_EnableVLAN(BOARD_PRIVATE_STRUCT *bps)
{
   u32   DeviceControlReg;
   u32   VftaReg;
   u32   VftaIndex;
   u32   BitInReg;
   u32   i;
   u32   TempRctlReg;
   iANSsupport_t *iANSdata;
   struct e1000_adapter *adapter = GIGABIT_ADAPTER_STRUCT(bps);
   iANSdata = ANS_PRIVATE_DATA_FIELD(bps);

   DEBUGLOG("bd_ans_hw_EnableVLAN: enter\n");

   //***************************************************************
   // If we're asked to enable vlan, but the VlanMode variable is
   // set to NONE, then just set back to default and return.
   //***************************************************************
   if (iANSdata->vlan_mode == IANS_VLAN_MODE_OFF) {
          DEBUGLOG("bd_ans_hw_EnableVLAN: vlan mode off, enabling priority only\n");
      return(bd_ans_hw_EnablePriorityRx(bps));
        }

   //***************************************************************
   // Read the RX control register.  We'll make changes and write
   // it back out at the end of the routine.
   //***************************************************************
   TempRctlReg = E1000_READ_REG(&adapter->hw, RCTL);


   if (iANSdata->tag_mode == IANS_BD_TAGGING_802_3AC)
   {
          DEBUGLOG("bd_ans_hw_EnableVLAN: enabling 802.3ac tagging\n");
      
      /******************************************************************
      ** Set the VLAN Ethertype (VET) register, so the hardware knows
      ** what Ethertype to look for to strip the Q-tag.
      ******************************************************************/
      E1000_WRITE_REG(&adapter->hw, VET, QTAG_TYPE);
   
      /**************************************************************
      ** Set VLAN Mode Enable bit in the Control register (CTRL.VME).
      ** This allows adding/stripping 802.3ac tags.
      **************************************************************/
      DeviceControlReg = E1000_READ_REG(&adapter->hw, CTRL);
      DeviceControlReg |= E1000_CTRL_VME;
      E1000_WRITE_REG(&adapter->hw, CTRL, DeviceControlReg);


      //**********************************************************
      // Set the VLAN Filter Table Array (VFTA) for the VLANs
      // that the adapter is a member of.  The VTFA is 128-32 bit
      // registers that we treat like a 4096 bit array (just like 
      // the MTA).  A .1q VLAN ID is 12 bits.  The upper 7 bits
      // will determine the VTFA register and lower 5 bits 
      // determine what bit in the register should be set.  
      //**********************************************************
          if (iANSdata->vlan_filtering_mode == IANS_VLAN_FILTERING_ON) {
                DEBUGLOG("bd_ans_hw_EnableVLAN: enabling vlan filtering\n");
        for (i=0; i < iANSdata->num_vlan_filter; i++)
        {
                VftaIndex = (iANSdata->VlanID[i] >> 5) & 0x7F;
                BitInReg = iANSdata->VlanID[i] & 0x1F;  

                // This is Read-Modify-Write operation
                VftaReg = E1000_READ_REG_ARRAY(&adapter->hw, VFTA, VftaIndex);
                VftaReg |= (1 << BitInReg);
                e1000_write_vfta(&adapter->hw, VftaIndex, VftaReg);
        }
          }

      //************************************************************
      // Set the VFE bit in the Receive Control register, and clear
      // the CFIEN bit.  This enables the VLAN filter, and does not
      // reject packets with the CFI bit set.  These bits will get
      // written out when we bring the adapter out of reset.
#ifndef EXTERAL_RELEASE
      // Clear the SISLH and ISLE bits to make sure we don't do ISL.
#endif
      //************************************************************
      TempRctlReg |= E1000_RCTL_VFE;
      TempRctlReg &= ~E1000_RCTL_CFIEN;

   } // end of if (adapter->VlanMode == VLAN_MODE_IEEE)
   


   //***************************************************************
   // Restore the saved Rctl register, along with our changed bits.
   //***************************************************************

   E1000_WRITE_REG(&adapter->hw, RCTL, TempRctlReg);
 
   return BD_ANS_SUCCESS;
}   

//*********************************************************************
// Name:         bd_ans_hw_DisableTagging
//
// Description:  Disables all tagging functions on the adapter.  Turns
//               off VLAN filtering and tag stripping on receive, and 
//               disables tagging on send.
//
// Author:       Mitch Williams
//
// Born on Date: 4/13/1999
//
// Arguments:    adapter - Pointer to HSM's adapter Data Space
//
// Returns:      ODISTAT
//
// Modification log:
// Date       Who      Description
// --------   ---      ------------------------------------------------
// 3/21/00  kcarlson    Modified for generic ans_hw.c file to share amongst
//                      all OS who support ANS.
//
//*********************************************************************
BD_ANS_STATUS 
bd_ans_hw_DisableTagging(BOARD_PRIVATE_STRUCT *bps)
{
   u32   DeviceControlReg;
   u32   TempRctlReg;
   struct e1000_adapter *adapter = GIGABIT_ADAPTER_STRUCT(bps);

    DEBUGLOG("bd_ans_hw_DisableTagging: enter\n");
    
   //***************************************************************
   // Read the RX control register.  We'll make changes and write
   // it back out at the end of the routine.
   //***************************************************************
   TempRctlReg = E1000_READ_REG(&adapter->hw, RCTL);


   //***************************************************************
   // Reset VLAN Mode Enable bit in the Control register (CTRL.VME).
   // This turns off adding/stripping 802.3ac tags.
   //***************************************************************
   DeviceControlReg = E1000_READ_REG(&adapter->hw, CTRL);
   DeviceControlReg &= ~E1000_CTRL_VME;
   E1000_WRITE_REG(&adapter->hw, CTRL, DeviceControlReg);

   //**********************************************************
   // Clear the VLAN Filter Table Array (VFTA).
   //**********************************************************

	e1000_clear_vfta(&adapter->hw);

   //************************************************************
   // Clear the VFE and ISLE bits in the Receive Control register.
   //************************************************************

   TempRctlReg &= ~E1000_RCTL_VFE;


   E1000_WRITE_REG(&adapter->hw, RCTL, TempRctlReg);
  
   return BD_ANS_SUCCESS;
}   

//*********************************************************************
// Name:         bd_ans_hw_EnablePriorityRx
//
// Description:  Enables priority tag stripping on the adapter.  Turns on 
//               VLAN filtering and tag stripping on receive, and enables
//               tagging on send.
//               This routine is based on Pat Connor's NDIS code.
//
// Author:       Mitch Williams
//
// Born on Date: 4/13/1999
//
// Arguments:    adapter - Pointer to HSM's adapter Data Space
//
// Returns:      ODISTAT    
//
// Modification log:
// Date       Who      Description
// --------   ---      ------------------------------------------------
// 3/21/00  kcarlson    Modified for generic ans_hw.c file to share amongst
//                      all OS who support ANS.
//
//*********************************************************************
BD_ANS_STATUS
bd_ans_hw_EnablePriorityRx(BOARD_PRIVATE_STRUCT *bps)
{
   u32   DeviceControlReg;
   u32   VftaReg;
   u32   TempRctlReg;
   struct e1000_adapter *adapter = GIGABIT_ADAPTER_STRUCT(bps);

   DEBUGLOG("bd_ans_hw_EnablePriorityRx: enter\n");
   TempRctlReg = E1000_READ_REG(&adapter->hw, RCTL);


   /******************************************************************
   ** Set the VLAN Ethertype (VET) register, so the hardware knows
   ** what Ethertype to look for to strip the Q-tag.
   ******************************************************************/
   E1000_WRITE_REG(&adapter->hw, VET, ETHERNET_IEEE_VLAN_TYPE);

   /**************************************************************
   ** Set VLAN Mode Enable bit in the Control register (CTRL.VME).
   ** This allows adding/stripping 802.3ac tags.
   **************************************************************/
   DeviceControlReg = E1000_READ_REG(&adapter->hw, CTRL);
   DeviceControlReg |= E1000_CTRL_VME;
   E1000_WRITE_REG(&adapter->hw, CTRL, DeviceControlReg);


   //**********************************************************
   // Set the VLAN Filter Table Array (VFTA) to only accept
   // packets on VLAN 0.  This will cause the hardware to 
   // reject all packets with valid VLAN tags, and only receive
   // packets with priority-only information.
   //**********************************************************

   VftaReg = E1000_READ_REG_ARRAY(&adapter->hw, VFTA, 0);
   VftaReg |= 1;
   e1000_write_vfta(&adapter->hw, 0, VftaReg);

   //************************************************************
   // Set the VFE bit in the Receive Control register, and clear
   // the CFIEN bit.  This enables the VLAN filter, and does not
   // reject packets with the CFI bit set.  These bits will get
   // written out when we bring the adapter out of reset.
   // Clear the SISLH and ISLE bits to make sure we don't do ISL.
   //************************************************************
   TempRctlReg |= E1000_RCTL_VFE;
   TempRctlReg &= ~E1000_RCTL_CFIEN;

   //***************************************************************
   // Restore the saved Rctl register, along with our changed bits.
   //***************************************************************

   E1000_WRITE_REG(&adapter->hw, RCTL, TempRctlReg);
   return BD_ANS_SUCCESS;
}   
#endif 


