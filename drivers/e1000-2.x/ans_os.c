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
* Abstract: iANS routines specific to linux                           *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/

#include "ans_driver.h"
#include "ans_os.h"


/* bd_ans_os_Ioctl()
**
**  This function will pull the IANS structures out of the ifr and pass
**  them to the generic ANS module for processing.
**
**  Arguments:  struct device *dev - pointer to the adapters device structure
**              struct ifreq *ifr - the request structure passed down from
**                                  upper layers.
**              int cmd - the number of the IOC to process.  This function
**                        will only process the IANS_BASE_SIOC command.
**
**  Returns:    int - 0 if successful, non-zero otherwise.
*/
int
bd_ans_os_Ioctl(struct device *dev, struct ifreq *ifr, int cmd)
{
    /* get the private data structure from the dev struct */
    BOARD_PRIVATE_STRUCT *bps = dev->priv;    
    IANS_BD_PARAM_HEADER *header =  (IANS_BD_PARAM_HEADER *)ifr->ifr_data;
    iANSsupport_t *iANSdata;
    BD_ANS_STATUS status;
    
    /* get a pointer to the ANS data struct from the ifr */
    iANSdata = ANS_PRIVATE_DATA_FIELD(bps);

    //DEBUGLOG("bd_ans_os_Ioctl: enter\n");

    /* switch on the command */
    switch(cmd) {
    case IANS_BASE_SIOC:
	status = bd_ans_ProcessRequest(bps, iANSdata, header);
	if (status == BD_ANS_SUCCESS)
	    return 0;
	/* some problem occured, return error value */
	return -EAGAIN;
    default:
	return -EOPNOTSUPP;
    }
    return 0;
}    


/* bd_ans_os_Transmit()
**
**  This function will get the required structures from the skb and
**  pass them to the generic bd_ans_Transmit routine for processing.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - pointer to the boards 
**                                          private data structure
**              HW_TX_DESCRIPTOR *txd - pointer to the hardware 
**                                      specific tx descriptor
**              struct sk_buff *skb - pointer to the skb which
**                                    describes this packet.
**
**  Returns:    int - 0 if successful, non-zero otherwise.
*/
int 
bd_ans_os_Transmit( BOARD_PRIVATE_STRUCT *bps, 
    HW_TX_DESCRIPTOR *txd,
    struct sk_buff **skb )
{
    UINT16 vlan_id;     /* don't know if I really need this */
    IANS_ATTR_HEADER *attr_head = iANSGetAttributeHeader(*skb);
    iANSsupport_t *iANSdata = ANS_PRIVATE_DATA_FIELD(bps);
    
    /* nothing special to do unless we are in attributed mode */
    
    /* call the bd_ans_Transmit routine to setup our frame for transmit */
    if (iANSdata->attributed_mode) {
        if (bd_ans_Transmit(bps,
	    iANSdata,
	    attr_head->pFirstTLV,
	    txd,
	    skb,
	    &vlan_id) == BD_ANS_FAILURE)
            return 1;                                                
        
    }        
    return 0;
}        



/* bd_ans_os_Receive()
**
**  This function will determine if the adapter is configured
**  for attributed mode, and call the generic ans routine
**  to add any needed tlvs if we are configured to do so.
**  It will also check the routing_mode flag to determine
**  if we need to check to see if we should route frames
**  to ANS.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - pointer to private data struct
**              HW_RX_DESCRIPTOR *rxd - pointer to hw specific frame descriptr
**              struct sk_buff *skb - the OS descriptor for this frame
**
**  Returns:    int - 0 if successful, non-zero otherwise
*/
int
bd_ans_os_Receive(BOARD_PRIVATE_STRUCT *bps,
    HW_RX_DESCRIPTOR *rxd,
    struct sk_buff *skb )
{
    UINT32 length;
    iANSsupport_t *iANSdata = ANS_PRIVATE_DATA_FIELD(bps);
    IANS_ATTR_HEADER *attr_head = iANSGetAttributeHeader(skb);
    struct sk_buff *dummy_tlv_ptr;
    struct device *dev = DRIVER_DEV_FIELD(bps);

    DEBUGLOG("bd_ans_os_Receive: enter\n");

    /* if we are in attributed mode, we need to fill out tlv structures */
    if (iANSdata->attributed_mode) {
	DEBUGLOG("bd_ans_os_Receive: in attributed mode\n");
        /* setup the TLV pointer first */
        attr_head->pFirstTLV = (Per_Frame_Attribute_Header *)
	    (((UCHAR *)iANSGetAddrOfPtrToFirstAttribute(skb)) 
		+ sizeof(IANS_ATTR_HEADER));


        if (bd_ans_Receive(bps,
	    iANSdata,
	    rxd,
	    skb->data,
	    skb,
	    &dummy_tlv_ptr, /* this isn't used by Linux */
	    &length) == BD_ANS_FAILURE) {
	    DEBUGLOG("bd_ans_os_Receive: Failed bd_ans_Receive\n");
	    return 1;	
	}
    } else {
        attr_head->pFirstTLV = NULL;
    }
    
    /* need to check to see if we are routing rx packets to ANS. If this
     * has been setup, then we need to replace the existing protocol
     * with the ANS protocol and store the original at the head of
     * the skb
     */
    if (iANSdata->routing_mode == IANS_ROUTING_ON) {
	DEBUGLOG("bd_ans_os_Receive: In routing mode\n");
	/* set the protocol here. the eth_type_trans routine
	 * changes the data pointer and so we want to do that after
	 * we have done any stripping of the packet 
	 */
	attr_head->OriginalProtocol = eth_type_trans(skb, dev);
        skb->protocol = IANS_FRAME_TYPE;
    } else {
	skb->protocol = eth_type_trans(skb, dev);
    }
    return 0;
}    



/* bd_ans_os_Watchdog()
**
**  This function will check on the status fields of the ANS
**  support structure and see if the status has changed since
**  the last time that it was checked.  If it has changed,
**  then we need to alert the ANS protocol somehow.
**
**  Arguments:  struct device *dev - pointer to the device structure
**              BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**
**  Returns: void
*/
void
bd_ans_os_Watchdog(struct device *dev, BOARD_PRIVATE_STRUCT *bps)
{
    IANS_BD_PARAM_STATUS current_status;	
    iANSsupport_t *piANSdata = ANS_PRIVATE_DATA_FIELD(bps);


    /* check ans communication protocol.  If we are not up, there is
     * nothing to do.
     */
    if ((piANSdata->iANS_status == IANS_COMMUNICATION_DOWN)
	|| (piANSdata->reporting_mode == IANS_STATUS_REPORTING_OFF))
        return;
    
    /* update the driver's status */
    bd_ans_drv_UpdateStatus(bps);
    
    /* fill out the current status */
    bd_ans_FillStatus(bps, ANS_PRIVATE_DATA_FIELD(bps), &current_status);
    
    /* compare the status to the last status update.  If they are different,
     * we need to send an indication.
     */
    /* Compare current status against previous one: if equal, just return */
    if(BD_ANS_BCMP(&piANSdata->prev_status, &current_status,
	sizeof(IANS_BD_PARAM_STATUS)) == BD_ANS_TRUE) {

	return;
    }
    
    /* if we are here, we need to send a status change notification */
    BD_ANS_BCOPY(&piANSdata->prev_status, &current_status, sizeof(IANS_BD_PARAM_STATUS));

    DEBUGLOG("bd_ans_os_Watchdog: sending notification\n");
    if (ans_notify)
	ans_notify(dev, IANS_IND_EXT_STATUS_CHANGE);
    DEBUGLOG("bd_ans_os_watchdog: done send\n");

    return;
}
            	 
#ifdef IANS_BASE_VLAN_TAGGING 
/* bd_ans_os_InsertQtagSW()
**
**  This function will insert the IEEE vlan id into the data portion of the 
**  packet.  
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - pointer to the boards private data
**                                          structure
**              struct sk_buff **skb - pointer to a pointer to the sk_buff
**                                     which describes this packet
**              UINT16 *vlanid - pointer to the vlan id to insert
**
**  Returns:    BD_ANS_STATUS - BD_ANS_FAILURE if a new skb needed to be 
**                              allocated but could not, BD_ANS_SUCCESS
**                              otherwise
*/    
BD_ANS_STATUS
bd_ans_os_InsertQtagSW(BOARD_PRIVATE_STRUCT *bps, struct sk_buff **skb, UINT16 *vlan_id)
{
    UINT32 frame_len;
    UINT32 count;
    UCHAR *from;
    UCHAR *to;
    peth_header_t peth_header;
    peth_vlan_header_t peth_vlan_header;
    struct sk_buff *new_skb;
    
    /* we can be guarenteed that there is headroom here because we are
     * blowing away the old TLV list since we don't need it anymore.
     */
    if (skb_headroom(*skb) < sizeof(x8021Q_tag_t) ) {
	DEBUGLOG("bd_ans_os_InsertQtagSW: inserting vlan into headroom\n");
	/* reallocate the skb */
	new_skb = skb_realloc_headroom((*skb), sizeof(x8021Q_tag_t));
        if (new_skb == NULL)
            return BD_ANS_FAILURE;
            
	/* return the old one */
	dev_kfree_skb(*skb);
	*skb = new_skb;
    }
    
    
    /* move the da/sa out of the way */
    from = (*skb)->data;
    to = ((UCHAR *)(*skb)->data) - sizeof(x8021Q_tag_t);
    for (count = 0; count < (ETHERNET_ADDRESS_LENGTH*2); count++)
	*to++ = *from++;
    
    /* adjust the data pointer to new spot */
    skb_push(*skb,sizeof(x8021Q_tag_t)); 
    
    /* insert the vlan id in the proper place */
    peth_vlan_header = (peth_vlan_header_t)(*skb)->data;
    peth_vlan_header->Qtag.EtherType = htons(QTAG_TYPE);
    peth_vlan_header->Qtag.VLAN_ID = htons(*vlan_id);
    return BD_ANS_SUCCESS;        
}


/* bd_ans_os_StripQtagSW()
**
**  This routine will strip a IEEE tag out of the data area of the 
**  skb.  We assume that the data pointer is still pointing to the
**  raw ethernet data (i.e. it better be!)
**
**  Arguments:  struct sk_buff *skb - pointer to the sk_buff which
**                                    describes this packet
**  Returns:    void - you get what you asked for, no checks to see
**                     if it is a valid vlan packet.
*/
void
bd_ans_os_StripQtagSW(struct sk_buff *skb)
{
    unsigned char *to;
    unsigned char *from;
    eth_vlan_header_t *header;
    
    header = (eth_vlan_header_t *) skb->data;
    
    /* start from the last byte of the source address and copy to
     * the last byte of the qtag.
     */
    from = &(header->eth_src[5]);
    to = from + sizeof(x8021Q_tag_t);
    while (from >= (unsigned char *)header)
        *to-- = *from--;
        
    /* reset the data to 4 bytes above what it was. */  
    skb_pull(skb, sizeof(x8021Q_tag_t));      
}


#endif


/* bd_ans_os_AllocateTLV()
**
**  This function will just set the tlv pointer to the proper place
**  to begin copying TLV information.  Under Linux, this function
**  doesn't do much, it is more complex under other OS.
**
**  struct sk_buff *frame - pointer to the sk_buff which describes
**                          the frame to be passed up.
**  struct sk_buff **tlv  - address of the sk_buff which will contain
**                          the tlv info.
**  
**  Returns:  BD_ANS_SUCCESS - always succeeds for now.
*/
BD_ANS_BOOLEAN
bd_ans_os_AllocateTLV(struct sk_buff *frame, struct sk_buff **tlv)
{
    /* since under linux we have our tlv at the head of the
     * frame data, we use the same sk_buff for the tlv as
     * the frame data and do not need to do any new allocation 
     */
    *tlv = frame;
    return BD_ANS_SUCCESS;
}



/* bd_ans_os_AttributeFill()
**
**  This function will serve as a translation layer between the generic
**  attribute fill routine and the OS specific data structures.  Tell
**  the generic routines where to fill in the TLV information.
**
**  Arguments:  iANS_Attribute_ID attr_id - the id of the attribute
**              struct sk_buff *skb - the skb which describes the tlv list
**              UINT32 prev_tlv_length - this will tell the routine how 
**                                       many bytes to skip to avoid 
**                                       writing over previous TLV information
**              void *data             - pointer any associated values that
**                                       belong to this TLV.
**  Returns:    UINT32 - the length of the new TLV
*/ 
UINT32
bd_ans_os_AttributeFill(iANS_Attribute_ID attr_id, 
    struct sk_buff *skb, 
    UINT32 prev_tlv_length,
    void *data)
{
    int tlv_length = 0;
    Per_Frame_Attribute_Header *header;

    /* we need to skip any previously filled in attributes so that we 
     * don't blow them away. 
     */
    header = iANSGetAttributeHeader(skb)->pFirstTLV;
    header = 
	(Per_Frame_Attribute_Header *)(((UINT8 *)header) + prev_tlv_length);
    tlv_length = bd_ans_AttributeFill(attr_id, 
	header, 
	data);
    /* we don't adjust the len field because the len is in relation to the 
     * frame data only.
     */

    return tlv_length;
}    
 
 

/* bd_ans_os_ReserveSpaceForANS()
**
** ANS requires that we have space in the skb for the following:
** 
**  pointer to TLV list
**  old protocol id
**  TLV list with at most 2 TLVs for now
** 
**  This function abstracts the call to skb_reserve so that if the
**  amount of space that needs to be reserved changes, this is 
**  only needed to be updated in the shared code and not in each 
**  individual driver.
**
**  Arguments:  struct sk_buff *skb - the sk_buff which is being
**                                    adjusted.
**
**  Returns:    void.
*/  
void 
bd_ans_os_ReserveSpaceForANS(struct sk_buff *skb)
{
    /* (??? what is the best way to
     * handle this variable number of tlv's? Perhaps ANS can set this 
     * number for us in a query or something, or as a define in the 
     * base_comm.h file...)
     */
    skb_reserve(skb, BD_ANS_INFO_SIZE);
}
       
