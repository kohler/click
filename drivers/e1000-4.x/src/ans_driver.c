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

/* driver specific routines for the linux gigabit driver */
#include "ans_driver.h"
#include "base_comm.h"

/* bd_ans_drv_InitANS()
**
**  This function should be called at driver Init time to set the pointers
**  in the iANSsupport_t structure to the driver's current pointers.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - private data struct
**              iANSsupport_t *iANSdata - iANS support structure.
**
**  Returns:  void
**
*/
void
bd_ans_drv_InitANS(BOARD_PRIVATE_STRUCT *bps, 
		   iANSsupport_t *iANSdata)
{
    bd_ans_Init(iANSdata);
    
    /* set all the required status fields to this driver's 
     * status fields.  remove these comments when you are done.
     */
    iANSdata->link_status = &(bps->ans_link);
    iANSdata->line_speed  = &(bps->ans_speed);
    iANSdata->duplex      = &(bps->ans_duplex);
    iANSdata->hw_fail   = NULL;
    iANSdata->suspended = &(bps->ans_suspend);
    iANSdata->in_reset  = &(bps->ans_suspend);
}                            



/* bd_ans_drv_UpdateStatus()
**
**  This function should update the driver board status in the iANSsupport
**  structure for this adapter
**
**  Arguments: BOARD_PRIVATE_STRUCT *bps - board private structure
**
**  Returns:  void
*/
void
bd_ans_drv_UpdateStatus(BOARD_PRIVATE_STRUCT *bps)
{
	bps->ans_link = netif_carrier_ok(bps->netdev) ? IANS_STATUS_LINK_OK :
			                                IANS_STATUS_LINK_FAIL;
	bps->ans_speed   = bps->link_speed;
	bps->ans_duplex  = bps->link_duplex;
	bps->ans_suspend = FALSE;
	return;	
}


#ifdef IANS_BASE_VLAN_TAGGING
/* bd_ans_drv_ConfigureTagging()
**
**  This function will call the HW specific functions to configure
**  the adapter to operate in tagging mode.  This function can also
**  be called to disable tagging support.  
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS if the adapter was configured
**                              BD_ANS_FAILURE if the adapter was not  
*/
BD_ANS_STATUS 
bd_ans_drv_ConfigureTagging(BOARD_PRIVATE_STRUCT *bps)
{
	/* this routine should call the hardware specific routines for
	 * configuring tagging.  Note that this could be the same 
	 * routine as the vlan configure routine (bd_ans_hw_EnableVlan)
	 * or bd_ans_hw_DisableTagging depending on how the 
     * tag_mode flags are set.  The driver should not modify
     * the flag
     */
	struct e1000_adapter * adapter = GIGABIT_ADAPTER_STRUCT(bps);
	piANSsupport_t iANSdata = ANS_PRIVATE_DATA_FIELD(bps);
	
	/* Check the requested mode, and if the NIC is not already operating in
	 * that mode then call either EnableVLAN or DisableTagging */	
	switch ((IANS_BD_TAGGING_MODE)iANSdata->tag_mode) {
	case IANS_BD_TAGGING_NONE:
		if(adapter->tag_mode != IANS_BD_TAGGING_NONE) {
			bd_ans_hw_DisableTagging(bps);
			adapter->tag_mode = IANS_BD_TAGGING_NONE;
		}
		break;
	case IANS_BD_TAGGING_802_3AC:
		if(adapter->tag_mode != IANS_BD_TAGGING_802_3AC &&
		   adapter->hw.mac_type >= e1000_82543) {
			bd_ans_hw_EnableVLAN(bps);
			adapter->tag_mode = IANS_BD_TAGGING_802_3AC;
		}
		break;
	default:
		return BD_ANS_FAILURE;
	}
	return BD_ANS_SUCCESS;
}


/* bd_ans_drv_ConfigureVlanTable()
**
**  This function will call the HW specific functions to configure the
**  adapter to do vlan filtering in hardware.  This function call also
**  be called to disable vlan filtering support
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**                 
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS if the adapter was configured
**                              BD_ANS_FAILURE otherwise
*/ 
BD_ANS_STATUS
bd_ans_drv_ConfigureVlanTable(BOARD_PRIVATE_STRUCT *bps)
{
	/* this function should call the hardware specific routines 
	 * for configuring the vlan table - note that this can be
	 * the same routines for configuring plain ole vlan (bd_ans_hw_EnableVlan)
     * or bd_ans_hw_DisableTagging depending on how the vlan_mode
     * and tag_mode flags are set.  The driver should not modify
     * the flag
	 */
	/* Similar to ConfigureVlan, but always call EnableVlan even if the NIC is
	 * already in VLAN tagging mode in order to rebuild the VLAN Table */ 
	piANSsupport_t iANSdata = ANS_PRIVATE_DATA_FIELD(bps);

	if(iANSdata->vlan_filtering_mode == IANS_VLAN_FILTERING_ON) {
			return bd_ans_hw_EnableVLAN(bps);
	} else {
			return bd_ans_hw_DisableTagging(bps);
	}
}


/* bd_ans_drv_ConfigureVlan()
**
**  This function will call the HW specific functions to configure the
**  adapter to operate in vlan mode. This function can also be called
**  to disable vlan mode.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**                 
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS if the adapter was configured
**                              BD_ANS_FAILURE otherwise
*/ 
BD_ANS_STATUS
bd_ans_drv_ConfigureVlan(BOARD_PRIVATE_STRUCT *bps)
{
	/* this function should call the hardware specific routines
	 * to configure the adapter in vlan mode (bd_ans_hw_EnableVlan)
	 * or bd_ans_hw_DisableTagging depending on how the vlan_mode
     * and tag_mode flags are set.  The driver should not modify
     * the flag
     */
	/* seems the same to me as ConfigureTagging (for 8254x hw at least) CL */
	return bd_ans_drv_ConfigureTagging(bps);
}
#endif

/* bd_ans_drv_StopWatchdog()
**
**  Since the linux driver already has a watchdog routine, we just need to
**  set a flag to change the code path in the watchdog routine to not call
**  the bd_ans_os_Watchdog() procedure.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - adapter private data
**
**  Returns:  void
*/
void
bd_ans_drv_StopWatchdog(BOARD_PRIVATE_STRUCT *bps)
{
    /* set a flag to indicate that we no longer need to call
    ** the bd_ans_os_Watchdog routine.
    */
	bps->iANSdata->reporting_mode = IANS_STATUS_REPORTING_OFF;
}


/* bd_ans_drv_StopPromiscuousMode()
**
**  The linux driver does not support this.
*/
BD_ANS_STATUS
bd_ans_drv_StopPromiscuousMode(BOARD_PRIVATE_STRUCT *bps)
{
    return BD_ANS_FAILURE;
}


/* bd_ans_drv_StartWatchdog()
**
**  Since the linux driver already has a watchdog routine started,
**  we just need to set a flag to change the code path to call the
**  bd_ans_os_Watchdog routine from the current watchdog routine.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - private data structure.
** 
**  Returns:  u32 - non-zero indicates success.
*/
u32 
bd_ans_drv_StartWatchdog(BOARD_PRIVATE_STRUCT *bps)
{
    /* set your flag to indicate that the watchdog routine should
    ** call ans_bd_os_Watchdog().
    */
	bps->iANSdata->reporting_mode = IANS_STATUS_REPORTING_ON;
    
    /* return a non-zero value */
    return (1);
}

u32
bd_ans_drv_hw_flags(BOARD_PRIVATE_STRUCT *bps)
{
	u16 data;
	if(bps->hw.mac_type < e1000_82544)
		return IANS_BD_FLAG4;

	if(e1000_read_eeprom(&bps->hw, 3, &data) != 0)
		return 0;

	if((data & 0x0600) == 0x0400)
		return IANS_BD_FLAG4;
	else
		return 0;
}

