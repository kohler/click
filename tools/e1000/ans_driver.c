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

/* driver specific routines for the linux gigabit driver */
#include <ans_driver.h>
#include <base_comm.h>

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
    iANSdata->link_status = (UINT32 *)&(((bd_config_t *)bps)->bddp->ans_link);
    iANSdata->line_speed  = &(((bd_config_t *)bps)->bddp->ans_speed);
    iANSdata->duplex      = &(((bd_config_t *)bps)->bddp->ans_duplex);
	iANSdata->hw_fail   = NULL;
    iANSdata->suspended = &(((bd_config_t *)bps)->bddp->AdapterStopped);
    iANSdata->in_reset  = &(((bd_config_t *)bps)->bddp->AdapterStopped);
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
    /* the linux gigabit driver doesn't need to do any updating here
    ** since its status fields are already updated in the watchdog routine
    ** before this function is called.
    */
   
	return;	
    /*iANSsupport_t *iANSdata = ANS_PRIVATE_DATA_FIELD(bps); */
 
    /* update the driver's current status if needed */
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
	PADAPTER_STRUCT Adapter = GIGABIT_ADAPTER_STRUCT(bps);
	piANSsupport_t iANSdata = ANS_PRIVATE_DATA_FIELD(bps);
	
	/* Check the requested mode, and if the NIC is not already operating in
	 * that mode then call either EnableVLAN or DisableTagging */	
	switch ((IANS_BD_TAGGING_MODE)iANSdata->tag_mode) {
	case IANS_BD_TAGGING_NONE:
		if(Adapter->tag_mode != IANS_BD_TAGGING_NONE) {
			bd_ans_hw_DisableTagging(bps);
			Adapter->tag_mode = IANS_BD_TAGGING_NONE;
		}
		break;
	case IANS_BD_TAGGING_802_3AC:
		if(Adapter->tag_mode != IANS_BD_TAGGING_802_3AC &&
		   Adapter->MacType >= MAC_LIVENGOOD) {
			bd_ans_hw_EnableVLAN(bps);
			Adapter->tag_mode = IANS_BD_TAGGING_802_3AC;
		}
		break;
	case IANS_BD_TAGGING_ISL:
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
VOID
bd_ans_drv_StopWatchdog(BOARD_PRIVATE_STRUCT *bps)
{
    /* set a flag to indicate that we no longer need to call
    ** the bd_ans_os_Watchdog routine.
    */
	((bd_config_t *)bps)->iANSdata->reporting_mode = IANS_STATUS_REPORTING_OFF;
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
**  Returns:  UINT32 - non-zero indicates success.
*/
UINT32 
bd_ans_drv_StartWatchdog(BOARD_PRIVATE_STRUCT *bps)
{
    /* set your flag to indicate that the watchdog routine should
    ** call ans_bd_os_Watchdog().
    */
	((bd_config_t *)bps)->iANSdata->reporting_mode = IANS_STATUS_REPORTING_ON;
    
    /* return a non-zero value */
    return (1);
}

