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

/* hardware specific routines for the gigabit adapter */
#include <ans_driver.h>


/* bd_ans_hw_available_speeds()
**
**  This function will determine the speed capabilities of this adapter
**  based on the phy type.
**
**  Arguments:  UINT32 phy - the phy id of the adapter
**
**  Returns:    UINT32     - the available speeds of the driver.
*/
UINT32
bd_ans_hw_available_speeds(UINT32 phy)
{
    UINT32 speeds;
    
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
    return ((rxd->ReceiveStatus & E1000_RXD_STAT_VP)?BD_ANS_TRUE:BD_ANS_FALSE);
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
bd_ans_hw_InsertQtagHW(BOARD_PRIVATE_STRUCT *bps, HW_TX_DESCRIPTOR *txd, UINT16 *vlanid)
{
    iANSsupport_t *iANSdata;

    iANSdata = ANS_PRIVATE_DATA_FIELD(bps);

    DEBUGLOG("bd_ans_hw_InsertQtagHW: enter\n");

    /* tell hardware to add tag */
    txd->Lower.DwordData |= E1000_TXD_CMD_ISLVE;

    /* set the vlan id */
    txd->Upper.Fields.Special = *vlanid;

    /* if we are in ISL mode, also set IFCS to make it
     * generate the internal checksum
     */
    if (iANSdata->tag_mode == IANS_BD_TAGGING_ISL) {
    	txd->Lower.DwordData |= E1000_TXD_CMD_IFCS;
    }
    return BD_ANS_SUCCESS;
}



/* bd_ans_hw_GetVlanId()
**
**  This function will retrieve the vlan id from the receive descriptor
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data structure
**              HW_RX_DESCRIPTOR *rxd     - the adapter's receive descriptor
**
**  Returns:    UINT16 - the vlan id (masked off priority)
*/
UINT16 
bd_ans_hw_GetVlanId(BOARD_PRIVATE_STRUCT *bps,
			        HW_RX_DESCRIPTOR *rxd)
{
    UINT16 VlanId;
	ADAPTER_STRUCT *Adapter = GIGABIT_ADAPTER_STRUCT(bps);
    
    DEBUGLOG("bd_ans_hw_GetVlanId: enter\n");
    
    // The packet has a tag, so extract it.
    // the first rev had the special field byte swapped
    if (Adapter->MacType != MAC_LIVENGOOD)
    {
		DEBUGLOG("bd_ans_hw_GetVlanId: getting vlanid on WiseMan\n");
        VlanId = (BYTE_SWAP_WORD(rxd->Special) & 
                 E1000_RXD_SPC_VLAN_MASK);
    }
    else
    {
        VlanId = (rxd->Special & 
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
// Arguments:    Adapter - Pointer to HSM's Adapter Data Space
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
   UINT32   DeviceControlReg;
   UINT32   VftaReg;
   UINT32   VftaIndex;
   UINT32   BitInReg;
   UINT32   i;
   UINT32   TempRctlReg;
   UINT32   PciCommandWord;
   iANSsupport_t *iANSdata;
   ADAPTER_STRUCT *Adapter = GIGABIT_ADAPTER_STRUCT(bps);
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
   TempRctlReg = E1000_READ_REG(Rctl);


   if (iANSdata->tag_mode == IANS_BD_TAGGING_802_3AC)
   {
	  DEBUGLOG("bd_ans_hw_EnableVLAN: enabling 802.3ac tagging\n");
      
      /******************************************************************
      ** Set the VLAN Ethertype (VET) register, so the hardware knows
      ** what Ethertype to look for to strip the Q-tag.
      ******************************************************************/
      E1000_WRITE_REG(Vet, QTAG_TYPE);
   
      /**************************************************************
      ** Set VLAN Mode Enable bit in the Control register (CTRL.VME).
      ** This allows adding/stripping 802.3ac tags.
      **************************************************************/
      DeviceControlReg = E1000_READ_REG(Ctrl);
      DeviceControlReg |= E1000_CTRL_VME;
      E1000_WRITE_REG(Ctrl, DeviceControlReg);


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
         	VftaReg = E1000_READ_REG(Vfta[VftaIndex]);
         	VftaReg |= (1 << BitInReg);
         	E1000_WRITE_REG(Vfta[VftaIndex], VftaReg);
      	}
	  }

      //************************************************************
      // Set the VFE bit in the Receive Control register, and clear
      // the CFIEN bit.  This enables the VLAN filter, and does not
      // reject packets with the CFI bit set.  These bits will get
      // written out when we bring the adapter out of reset.
      // Clear the SISLH and ISLE bits to make sure we don't do ISL.
      //************************************************************
      TempRctlReg |= E1000_RCTL_VFE;
      TempRctlReg &= ~E1000_RCTL_CFIEN;
      TempRctlReg &= ~E1000_RCTL_SISLH;
      TempRctlReg &= ~E1000_RCTL_ISLE;
#ifdef UW78
      TempRctlReg &= ~E1000_RCTL_VFE;
#endif

   } // end of if (Adapter->VlanMode == VLAN_MODE_IEEE)
   else  // VlanMode is ISL
   {
	  DEBUGLOG("bd_ans_hw_EnableVLAN: enabling ISL vlan\n");

      /**************************************************************
      ** Clear VLAN Mode Enable bit in the Control register (CTRL.VME).
      **************************************************************/
      DeviceControlReg = E1000_READ_REG(Ctrl);
      DeviceControlReg &= ~E1000_CTRL_VME;
      E1000_WRITE_REG(Ctrl, DeviceControlReg);
      
      //************************************************************
      // Set the SISLH and ISLE bits in the Receive Control register.
      // This allows reception of ISL packets, and stripping of
      // headers. We also clear the VFE and CFIEN bits to turn off
      // IEEE.
      //************************************************************
      TempRctlReg |= E1000_RCTL_SISLH;
      TempRctlReg |= E1000_RCTL_ISLE;
      TempRctlReg &= ~E1000_RCTL_VFE;
      TempRctlReg &= ~E1000_RCTL_CFIEN;
   }
   


   //***************************************************************
   // Restore the saved Rctl register, along with our changed bits.
   //***************************************************************

   E1000_WRITE_REG(Rctl, TempRctlReg);
 
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
// Arguments:    Adapter - Pointer to HSM's Adapter Data Space
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
   UINT32   DeviceControlReg;
   UINT32   i;
   UINT32   TempRctlReg;
   UINT32   PciCommandWord;
   ADAPTER_STRUCT *Adapter = GIGABIT_ADAPTER_STRUCT(bps);

    DEBUGLOG("bd_ans_hw_DisableTagging: enter\n");
    
   //***************************************************************
   // Read the RX control register.  We'll make changes and write
   // it back out at the end of the routine.
   //***************************************************************
   TempRctlReg = E1000_READ_REG(Rctl);


   //***************************************************************
   // Reset VLAN Mode Enable bit in the Control register (CTRL.VME).
   // This turns off adding/stripping 802.3ac tags.
   //***************************************************************
   DeviceControlReg = E1000_READ_REG(Ctrl);
   DeviceControlReg &= ~E1000_CTRL_VME;
   E1000_WRITE_REG(Ctrl, DeviceControlReg);

   //**********************************************************
   // Clear the VLAN Filter Table Array (VFTA).
   //**********************************************************

   for (i=0; i<E1000_VLAN_FILTER_TBL_SIZE; i++)
      E1000_WRITE_REG(Vfta[i], 0);


   //************************************************************
   // Clear the VFE and ISLE bits in the Receive Control register.
   //************************************************************

   TempRctlReg &= ~E1000_RCTL_VFE;
   TempRctlReg &= ~E1000_RCTL_ISLE;
   TempRctlReg &= ~E1000_RCTL_SISLH;


   E1000_WRITE_REG(Rctl, TempRctlReg);
  
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
// Arguments:    Adapter - Pointer to HSM's Adapter Data Space
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
   UINT32   DeviceControlReg;
   UINT32   VftaReg;
   UINT32   TempRctlReg;
   UINT32   PciCommandWord;
   ADAPTER_STRUCT *Adapter = GIGABIT_ADAPTER_STRUCT(bps);

   DEBUGLOG("bd_ans_hw_EnablePriorityRx: enter\n");
   TempRctlReg = E1000_READ_REG(Rctl);


   /******************************************************************
   ** Set the VLAN Ethertype (VET) register, so the hardware knows
   ** what Ethertype to look for to strip the Q-tag.
   ******************************************************************/
   E1000_WRITE_REG(Vet, ETHERNET_IEEE_VLAN_TYPE);

   /**************************************************************
   ** Set VLAN Mode Enable bit in the Control register (CTRL.VME).
   ** This allows adding/stripping 802.3ac tags.
   **************************************************************/
   DeviceControlReg = E1000_READ_REG(Ctrl);
   DeviceControlReg |= E1000_CTRL_VME;
   E1000_WRITE_REG(Ctrl, DeviceControlReg);


   //**********************************************************
   // Set the VLAN Filter Table Array (VFTA) to only accept
   // packets on VLAN 0.  This will cause the hardware to 
   // reject all packets with valid VLAN tags, and only receive
   // packets with priority-only information.
   //**********************************************************

   VftaReg = E1000_READ_REG(Vfta[0]);
   VftaReg |= 1;
   E1000_WRITE_REG(Vfta[0], VftaReg);

   //************************************************************
   // Set the VFE bit in the Receive Control register, and clear
   // the CFIEN bit.  This enables the VLAN filter, and does not
   // reject packets with the CFI bit set.  These bits will get
   // written out when we bring the adapter out of reset.
   // Clear the SISLH and ISLE bits to make sure we don't do ISL.
   //************************************************************
   TempRctlReg |= E1000_RCTL_VFE;
   TempRctlReg &= ~E1000_RCTL_CFIEN;
   TempRctlReg &= ~E1000_RCTL_SISLH;
   TempRctlReg &= ~E1000_RCTL_ISLE;

   //***************************************************************
   // Restore the saved Rctl register, along with our changed bits.
   //***************************************************************

   E1000_WRITE_REG(Rctl, TempRctlReg);
   return BD_ANS_SUCCESS;
}   
#endif 


