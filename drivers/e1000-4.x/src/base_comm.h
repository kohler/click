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
* Module Name:  base_comm.h                                           *
*                                                                     *
* Abstract: iANS to base communication defines                        *
*                                                                     *
* Environment:                                                        *
*                                                                     *
**********************************************************************/

#ifndef _IANS_BASE_COMM_H
#define _IANS_BASE_COMM_H


#include <linux/sockios.h> /* for SIOCDEVPRIVATE */

#define u16 __u16
#define u32 __u32
#define u8  __u8


/* Make sure all communications parties use the same packing mode
 * for the shared structures. */
#ifdef __ia64__
#pragma pack(8)
#else
#pragma pack(4)
#endif /*  __ia64__ */ 

#ifdef _IANS_MAIN_MODULE_C_
#define IntOrExt
#else
#define IntOrExt extern
#endif


/*--------------------------------------------------------------------*
 | PRIMITIVES baring iANS communications
 | =====================================
 *--------------------------------------------------------------------*/

/* The proprietary iANS IOCTL code */
#define IANS_BASE_SIOC          (SIOCDEVPRIVATE+1)
/* The proprietary event notifications code */
#define IANS_BASE_NOTIFY        (('S'<<24)|('N'<<16)|('A'<<8)|('i')) /* "iANS" */


/*------------------------------------------------------------------*
|   Communication version : 
|   this is the version of the communication protocol described in 
|   this header file. 
|   This information will be passed in the IANS_BD_IDENTIFY ioctl 
*------------------------------------------------------------------*/

/* iANS's communications version */
#define IANS_COMM_VERSION_MAJOR (u16)1
#define IANS_COMM_VERSION_MINOR (u16)0

/* Base driver's version */ 
#define IANS_BD_COMM_VERSION_MAJOR (u16)1
#define IANS_BD_COMM_VERSION_MINOR (u16)0

#define IANS_SIGNATURE_LENGTH 80

IntOrExt char IntelCopyrightString[IANS_SIGNATURE_LENGTH]
#ifdef _IANS_MAIN_MODULE_C_
= "Intel Copyright 1999, all rights reserved\n";
#else
;
#endif




/* ================================================================== *
 *                                                                    *
 *                               IOCTLs                               *
 *                                                                    *
 * ================================================================== */



/*--------------------------------------------------------------------*
 |                 Proprietary Opcodes
 *--------------------------------------------------------------------*/

typedef enum _IANS_BASE_OPCODE
{

    /* ----------  Basic Extension Commands  ---------- */
    IANS_OP_BD_IDENTIFY,          /* Identify BD to make sure it 
                                     supports iANS comm.*/
    /* BD fills Struct:  IANS_BD_PARAM_IDENTIFY */
    IANS_OP_BD_DISCONNECT,        /* Tell the BD that iANS is about to unload */
    IANS_OP_EXT_GET_CAPABILITY,   /* Get extended capabilities */
    /* BD fills struct: _IANS_BD_PARAM_EXT_CAP */
    IANS_OP_EXT_SET_MODE,         /* Set extended capabilities */
    /* iANS fills struct: _IANS_BD_PARAM_EXT_SET_MODE */
    IANS_OP_EXT_GET_STATUS,               /* Get status from base driver */
    /* BD fills status struct: IANS_BD_IOC_PARAM_STATUS */


    IANS_OP_ANS_SET_CB, /* pass ans function's pointers to base */

#ifdef IANS_BASE_ADAPTER_TEAMING
    /* ----------  Adapter Teaming Commands  ---------- */
    IANS_OP_IAT_FIRST=0x0100,     /* Skip over reserved area */
    /* There are no commands specific to adapter teaming */
    /* Capabilities are included in "Extended capabilities" */
#endif /* IANS_BASE_ADAPTER_TEAMING */

#ifdef IANS_BASE_VLAN_TAGGING
    /* ----------  VLan Tagging Commands  ---------- */
    IANS_OP_ITAG_FIRST=0x0200,          /* Skip over reserved area */
    IANS_OP_ITAG_GET_CAPABILITY,        /* Get tagging capabilities */
    /* BD fills struct: IANS_BD_PARAM_ITAG_CAP */
    IANS_OP_ITAG_SET_MODE,                      /* None/ISL/802.3ac */
    /* IANS fills struct: IANS_BD_PARAM_ITAG_SET_MODE */
#endif /* IANS_BASE_VLAN_TAGGING */

#ifdef IANS_BASE_VLAN_ID
    /* ----------  VLan ID Commands  ---------- */
    IANS_OP_IVLAN_ID_FIRST=0x0300,       /* Skip over reserved area */
    IANS_OP_IVLAN_ID_GET_CAPABILITY, /* Get VLan ID capabilities */
    /* BD fills struct: IANS_BD_PARAM_IVLAN_CAP */
    IANS_OP_IVLAN_ID_SET_MODE,           /* Set Vlan ID mode */
    /* iANS fills struct: IANS_BD_PARAM_IVLAN_SET_MODE*/
    IANS_OP_IVLAN_ID_SET_TABLE,          /* ID list */
    /* iANS fills struct: IANS_BD_PARAM_IVLAN_TABLE */
#endif /* IANS_BASE_VLAN_ID */

    /* ---------- */
    IANS_OP_COMMA       /* Dummy to satisfy last ifdef'ed commma */

} IANS_BASE_OPCODE, *PIANS_BASE_OPCODE;


/*--------------------------------------------------------------------*
 |                Enumerated types for field values
 *--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*
 |             Enumerated types for Status struct field values
 *--------------------------------------------------------------------*
 | In every field, if a base driver doesn't support a valid indication 
 | on that field it should set it to zero. This value was picked in 
 | every enumerated type to denote "not supported" value.
 *--------------------------------------------------------------------*/


/* Used in IANS_BD_PARAM_STATUS.LinkStatus */
typedef enum _IANS_BD_LINK_STATUS 
{
    IANS_STATUS_LINK_NOT_SUPPORTED = 0,
    IANS_STATUS_LINK_OK,
    IANS_STATUS_LINK_FAIL
} IANS_BD_LINK_STATUS, *PIANS_BD_LINK_STATUS;

/* Used in IANS_BD_PARAM_STATUS.Duplex */
typedef enum _IANS_BD_DUPLEX_STATUS 
{
    IANS_STATUS_DUPLEX_NOT_SUPPORTED = 0,
    IANS_STATUS_DUPLEX_HALF,
    IANS_STATUS_DUPLEX_FULL
} IANS_BD_DUPLEX_STATUS, *PIANS_BD_DUPLEX_STATUS;

/* Used in IANS_BD_PARAM_STATUS.LinkSpeed */

typedef enum _IANS_BD_LINK_SPEED {
    IANS_STATUS_LINK_SPEED_NOT_SUPPORTED = 0,
    IANS_STATUS_LINK_SPEED_1MBPS = 0x1,
    IANS_STATUS_LINK_SPEED_10MBPS = 0x2,
    IANS_STATUS_LINK_SPEED_100MBPS = 0x4,
    IANS_STATUS_LINK_SPEED_1000MBPS = 0x8
}IANS_BD_LINK_SPEED, *PIANS_BD_LINK_SPEED;


/* Used in IANS_BD_PARAM_STATUS.HardwareFailure */
typedef enum _IANS_BD_HW_STATUS {
    IANS_STATUS_HARDWARE_NOT_SUPPORTED = 0,
    IANS_STATUS_HARDWARE_OK,
    IANS_STATUS_HARDWARE_FAILURE
} IANS_BD_HW_STATUS, *PIANS_BD_HW_STATUS;

/* Used in IANS_BD_PARAM_STATUS.DuringResetProcess */
typedef enum _IANS_BD_RESET_STAGE {
    IANS_STATUS_RESET_NOT_SUPPORTED =0,
    IANS_STATUS_NOT_DURING_RESET,
    IANS_STATUS_DURING_RESET
} IANS_BD_RESET_STAGE, *PIANS_BD_RESET_STAGE;

/* Used in IANS_BD_PARAM_STATUS.Suspended */
typedef enum _IANS_BD_SUSPENDED_STAGE {
    IANS_STATUS_SUSPENDED_NOT_SUPPORTED =0,
    IANS_STATUS_NOT_SUSPENDED,
    IANS_STATUS_SUSPENDED
} IANS_BD_SUSPENDED_STAGE, *PIANS_BD_SUSPENDED_STAGE;

/* Rx and event notification routing mechanisms (bitmask ready) */
typedef enum _IANS_BD_ROUTING {
    IANS_ROUTING_NOT_SUPPORTED  = 0x00,
    IANS_ROUTING_RX_PROTOCOL    = 0x01,
} IANS_BD_ROUTING, *PIANS_BD_ROUTING;

typedef enum _IANS_BD_HOT_PLUG_STATUS {
    IANS_STATUS_HOT_PLUG_NOT_SUPPORTED = 0,
    IANS_STATUS_HOT_PLUG_NOT_DONE,
    IANS_STATUS_HOT_PLUG_WAS_DONE
} IANS_BD_HOT_PLUG_STATUS, *PIANS_BD_HOT_PLUG_STATUS;

/*--------------------------------------------------------------------*
 |         Enumerated types for general structs field values
 *--------------------------------------------------------------------*/

/* general typedef for various features - whether BD supports a feature 
   or not. To be used on "capabilites" structs */

typedef enum _IANS_BD_SUPPORT {
    IANS_BD_DOES_NOT_SUPPORT = 0,
    IANS_BD_SUPPORTS
} IANS_BD_SUPPORT, *PIANS_BD_SUPPORT;

/* general typedef for various features - whether iANS requests the BD 
   to support a feature or not. To be used on "set mode" structs. */
typedef enum _IANS_BD_REQUEST_SUPPORT {
    IANS_DONT_SUPPORT = 0,
    IANS_REQUEST_SUPPORT
} IANS_BD_REQUEST_SUPPORT, *PIANS_BD_REQUEST_SUPPORT;



/*--------------------------------------------------------------------*
 |                    Ioctl parameter structs
 |                    =======================
 | The following structures are used by the different IOCTLs to pass
 | parameters between the Base driver and iANS.
 *--------------------------------------------------------------------*/


/*--------------------------------------------------------------------*
 |                 Common Command/Parameter Header
 |                 ===============================
 | The 1st field of every parameter struct.
 *--------------------------------------------------------------------*/

typedef struct _IANS_BD_PARAM_HEADER
{
    /* Cast from IANS_BASE_OPCODE to insure forward size compatibility */
    /* u32 */ int    Opcode; 
} IANS_BD_PARAM_HEADER, *PIANS_BD_PARAM_HEADER;


/*--------------------------------------------------------------------*
 |                 Status result
 |                 =============
 | This struct is sent with as a response to GET_STATUS request.
 | The status struct contains a version number : in case we will wish to 
 | extend the status result, we will need a way to indicate which 
 | version of the status struct we support. 
 *--------------------------------------------------------------------*/
#define IANS_STATUS_VERSION        0x00000001

typedef struct _IANS_BD_PARAM_STATUS
{  
    u32     StatusVersion;       /* The version of this struct */
    
    u32     LinkStatus;          /* Cast from IANS_BD_LINK_STATUS */
    u32     LinkSpeed;           /* Cast from IANS_BD_LINK_SPEED */
    u32     Duplex;              /* Cast from IANS_BD_DUPLEX_STATUS  */
    u32     HardwareFailure;     /* Cast from IANS_BD_HW_FAILURE */
    u32     DuringResetProcess;  /* Cast from IANS_BD_RESET_STAGE  */
    u32     Suspended;           /* Cast from IANS_BD_SUSPENDED_STAGE  */

    u32     HotPlug;    /* Cast from IANS_BD_HOT_PLUG_STATUS  */
} IANS_BD_PARAM_STATUS, *PIANS_BD_PARAM_STATUS;

/*--------------------------------------------------------------------*
 |                 Status ioctl result
 |                 ===================
 | This structure is the struct sent as a response to the GET_STATUS 
 | request ioctl. It contains the ioctl header, and the status struct.
 *--------------------------------------------------------------------*/

typedef struct _IANS_BD_IOC_PARAM_STATUS
{
    IANS_BD_PARAM_HEADER           Header;          /* Common to all commands */
    IANS_BD_PARAM_STATUS   Status;
} IANS_BD_IOC_PARAM_STATUS, *PIANS_BD_IOC_PARAM_STATUS;



/*--------------------------------------------------------------------*
 |                      Indication 
 |                     ============                   
 | This enum value is sent by the notify call back function
 | called by the base driver for indication purposes.
 *--------------------------------------------------------------------*/

typedef enum _IANS_INDICATION 
{
    /* ----------  Basic Extention Indications  ---------- */
    IANS_IND_EXT_HWMODIFY,                          
                                   
    IANS_IND_EXT_STATUS_CHANGE,     /* Report a new status */
                                      
    IANS_IND_XMIT_QUEUE_FULL, // tell ANS to stop transmit through this member

    IANS_IND_XMIT_QUEUE_READY, // tell ANS to start transmit through this member
                                      
    IANS_IND_COMMA  /* Dummy to satisfy last ifdef'ed commma */
        
} IANS_INDICATION, *PIANS_INDICATION;




typedef struct _IANS_BD_ANS_SET_CB {
    IANS_BD_PARAM_HEADER           Header;
    void *notify;
} IANS_BD_ANS_SET_CB, *PIANS_BD_ANS_SET_CB;



/*--------------------------------------------------------------------*
 |                    Identify yourself struct
 |                    ========================
 | This struct is sent with the IANS_BD_IDENTIFY request. 
 | iANS fills its signature string and version number, and sends it to
 | the base driver. The base driver fills its own signature string and
 | version adn returns it.
 *--------------------------------------------------------------------*/

typedef struct _IANS_BD_PARAM_IDENTIFY
{
    IANS_BD_PARAM_HEADER           Header;          /* Common to all commands */

    u8      iANSSignature[IANS_SIGNATURE_LENGTH]; 
    /* iANS fills copyright string*/
    u8      BDSignature[IANS_SIGNATURE_LENGTH];   
    /* BD fills copyright string*/
    u32     iANSCommVersion;  /* iANS supported comm. version */
    /* Upper word = major version number */
    /* lower word = minor version number */
    u32     BDCommVersion;    /* Base driver supported comm. version */
} IANS_BD_PARAM_IDENTIFY, *PIANS_BD_PARAM_IDENTIFY;



/*--------------------------------------------------------------------*
 |                 Get Extended capabilities parameters struct
 |                 =========================================== 
 | This struct is sent with IANS_OP_EXT_GET_CAPABILITY - to be filled 
 | by the base driver and sent to iANS
 *--------------------------------------------------------------------*/ 
#define IANS_BD_FLAG1           0x0001
#define IANS_BD_FLAG2           0x0002
#define IANS_BD_FLAG3           0x0004
#define IANS_BD_FLAG4           0x0008
#define IANS_BD_FLAG5           0x0010
#define IANS_BD_FLAG6           0x0020
#define IANS_BD_FLAG7           0x0040
#define IANS_BD_FLAG8           0x0080


typedef struct _IANS_BD_PARAM_EXT_CAP
{
    IANS_BD_PARAM_HEADER           Header;                 /* Common to all commands */

    u32 BDCanSetMacAddress;    /* MAC Address setting - cast from 
                                     IANS_BD_SUPPORT*/
    u32 BDIansStatusVersion;    /* Status indication with iANS struct 
                                      - which version is supported  */
    u32 BDAllAvailableRouting;               /* Bitmask of all available Rx/Event
                                                 * routings. IANS_BD_ROUTING */

    u32     BDFlags;    /* The adapter's flags */

    u32     BDAllAvailableSpeeds; /* A bit mask of all available speeds */

} IANS_BD_PARAM_EXT_CAP, *PIANS_BD_PARAM_EXT_CAP;


/*--------------------------------------------------------------------*
 |                    Set Extended mode parameters struct
 |                    ======================================== 
 | This struct is sent with IANS_OP_EXT_SET_MODE - to be filled 
 | by iANS and sent to the base driver
 |
 | iANS tells the base driver whether to report its status through
 | the extended struct or not.
 *--------------------------------------------------------------------*/ 


typedef struct _IANS_BD_PARAM_EXT_SET_MODE
{
    IANS_BD_PARAM_HEADER           Header; /* Common to all commands */

    u32 BDIansStatusReport;   /* Ask the base driver to report status through 
                                  * status struct.  
                                  * Cast from IANS_BD_REQUEST. */

    u32 BDIansAttributedMode; /* Ask the base driver to send and receive
                                  * packets accompanied by a per-frame data structure
                                  * Cast from IANS_BD_REQUEST. */

    u32 BDIansRoutingMode;    /* Bitmask of one Rx and one IANS_BD_ROUTING */


} IANS_BD_PARAM_EXT_SET_MODE, *PIANS_BD_PARAM_EXT_SET_MODE;



#ifdef IANS_BASE_VLAN_TAGGING

/*--------------------------------------------------------------------*
 |                  Get Vlan tagging capabilities
 |                  =============================
 | This struct is sent with the IANS_OP_ITAG_GET_CAPABILITY ioctl, to be filled 
 | by the base driver.
 *--------------------------------------------------------------------*/

typedef struct _IANS_BD_PARAM_ITAG_CAP
{
    IANS_BD_PARAM_HEADER           Header;                 /* Common to all commands */

    u32                 ISLTagMode;         /* cast from IANS_BD_SUPPORT */
    u32                 IEEE802_3acTagMode; /* cast from IANS_BD_SUPPORT */

} IANS_BD_PARAM_ITAG_CAP, *PIANS_BD_PARAM_ITAG_CAP;

/*--------------------------------------------------------------------*
 |                  Set Vlan tagging mode
 |                  =====================
 | This struct is sent with the IANS_OP_ITAG_SET_MODE ioctl, to be filled 
 | by iANS.
 *--------------------------------------------------------------------*/

typedef enum _IANS_BD_TAGGING_MODE 
{
    IANS_BD_TAGGING_NONE =0,
    IANS_BD_TAGGING_802_3AC,
    IANS_BD_TAGGING_UNDEFINED
}IANS_BD_TAGGING_MODE , *PIANS_BD_TAGGING_MODE ;

typedef struct _IANS_BD_PARAM_ITAG_SET_MODE
{
    IANS_BD_PARAM_HEADER           Header;              /* Common to all commands */

    u32                SetTagMode;  /* cast from IANS_BD_TAGGING_MODE  */
} IANS_BD_PARAM_ITAG_SET_MODE, *PIANS_BD_PARAM_ITAG_SET_MODE;

#endif /* IANS_BASE_VLAN_TAGGING */


#ifdef IANS_BASE_VLAN_ID

/*--------------------------------------------------------------------*
 |            Get Vlan ID capabilities
 |            ========================
 | This struct is sent with the IANS_OP_IVLAN_ID_GET_CAPABILITY ioctl, 
 | to be filled by the base driver
 *--------------------------------------------------------------------*/

typedef struct _IANS_BD_PARAM_IVLAN_CAP
{
    IANS_BD_PARAM_HEADER           Header;                 /* Common to all commands */

    u32           VlanIDCapable;          /* Cast from IANS_BD_SUPPORT */
    u32           VlanIDFilteringAble;    /* Cast from IANS_BD_SUPPORT */
    u16           MaxVlanIDSupported;     /* Max. VLan ID supported by BD */
    u32           MaxVlanTableSize;       /* Max. number of VLan IDs in a table */
} IANS_BD_PARAM_IVLAN_CAP, *PIANS_BD_PARAM_IVLAN_CAP;

/*--------------------------------------------------------------------*
 |           Set Vlan ID mode
 |           ================ 
 | This struct is sent with the IANS_OP_IVLAN_ID_SET_MODE ioctl, filled
 | by the iANS 
 *--------------------------------------------------------------------*/ 

typedef struct _IANS_BD_PARAM_IVLAN_SET_MODE
{
    IANS_BD_PARAM_HEADER           Header;          /* Common to all commands */
    u32           VlanIDRequest;           /* Cast from IANS_BD_REQUEST */
    u32           VlanIDFilteringRequest;  /* Cast from IANS_BD_REQUEST */
} IANS_BD_PARAM_IVLAN_SET_MODE, *PIANS_BD_PARAM_IVLAN_SET_MODE;

/*--------------------------------------------------------------------*
 |          Set Vlan ID filtering table
 |          ===========================
 | This struct is sent with the IANS_OP_IVLAN_ID_SET_TABLE request
 *--------------------------------------------------------------------*/

typedef struct _IANS_BD_PARAM_IVLAN_TABLE
{
    IANS_BD_PARAM_HEADER           Header;         /* Common to all commands */

    u32        VLanIDNum;       /* Number of VLan IDs defined in 
                                      this table */
    u16        *VLanIDTable;  /* Beginning of ID list. 
                                    * iANS will allocate enough space 
                                    * for the whole table, and it will 
                                    * start from this field - we don't 
                                    * want to force this struct to be 
                                    * as big as the maximum number of 
                                    * VLan IDs. */

} IANS_BD_PARAM_IVLAN_TABLE, *PIANS_BD_PARAM_IVLAN_TABLE;

#endif /* IANS_BASE_VLAN_ID */



/*--------------------------------------------------------------------*
 |                PER_FRAME_ATTRIBUE_HEADER
 |                *************************
 | This header will be included in every TLV 
 *--------------------------------------------------------------------*/

typedef struct _Per_Frame_Attribute_Header
{
    u32               AttributeID;   /* Indicates which kind of data is contained
                                           in this field */
    u32               AttributeLength; /* Length of this attribute */
} Per_Frame_Attribute_Header, *pPer_Frame_Attribute_Header;


/* ================================================================= *
 *                                                                   *
 *                           Per-Message Attributes                  *
 *                                                                   *
 * ================================================================= */

typedef struct _IANS_ATTR_HEADER
{
    pPer_Frame_Attribute_Header         pFirstTLV;              /* NULL if not attributed */
    u32                                      OriginalProtocol;
} IANS_ATTR_HEADER, *PIANS_ATTR_HEADER;

/* Turn into a legal pointer */
#if defined(__i386__)
#define CelingAlignPtr(p)       ( p )
#else
#define CelingAlignPtr(p)       ( p )
#endif

/* The attribute header is kept at the beginning of the allocated buffer */
#define iANSGetReceiveAttributeHeader(skb) \
                ( (IANS_ATTR_HEADER*) CelingAlignPtr ( (char*)((skb)->head) ) ) 
#define iANSGetTransmitAttributeHeader(skb) \
                ( (IANS_ATTR_HEADER*) CelingAlignPtr ( (char*)((skb)->cb) ) ) 





/*--------------------------------------------------------------------*
 |                Attribute IDs
 |                *************
 | These values indicate which type of attribute is contained in a
 | certain TLV.
 *--------------------------------------------------------------------*/

typedef enum _iANS_Attribute_ID
{
    IANS_ATTR_LAST_ATTR=0, /* Marks the last attribute in a list */
    IANS_ATTR_DUMMY,       /* Non-initiating side should ignore this attribute */

#ifdef IANS_BASE_VLAN_ID 
    IANS_ATTR_VLAN_FIRST = 0x100, /* skip over reserved area */
    IANS_ATTR_VLAN_ID,     /* This attribute contains the VLan ID */
#endif /* IANS_BASE_VLAN_ID */ 

#ifdef IANS_BASE_VLAN_TAGGING
    IANS_ATTR_TAGGING_FIRST = 0x200, /* skip over reserved area */
    IANS_ATTR_TAGGING_UNTAGGED,     /* This determines that frame is untagged */
#endif /* IANS_BASE_VLAN_TAGGING */ 

    IANS_ATTR_COMMA
} iANS_Attribute_ID,  *piANS_Attribute_ID; 




#ifdef IANS_BASE_VLAN_ID

/*-------------------------------------------------------------------
|                       VLAN_ID_PER_FRAME_INFO
|                       **********************
| This structure contains only the VLanID per-frame information.
 -------------------------------------------------------------------*/
typedef struct _VLAN_ID_Per_Frame_Info 
{
    Per_Frame_Attribute_Header  AttrHeader;

    u16 VLanID;
    u8  Padding[2];

} VLAN_ID_Per_Frame_Info  , *pVLAN_ID_Per_Frame_Info ;

#endif /* IANS_BASE_VLAN_ID */


#ifdef IANS_BASE_VLAN_TAGGING

/*-------------------------------------------------------------------
|                       Untagged_Attribute
|                       ******************
| This structure is for the "untagged" attribute (used to indicate
| that frame is untagged).
 -------------------------------------------------------------------*/

typedef struct _Untagged_Attribute
{
    Per_Frame_Attribute_Header  AttrHeader;

} Untagged_Attribute  , *pUntagged_Attribute ;

#endif /* IANS_BASE_VLAN_TAGGING */


/*-------------------------------------------------------------------
|                       LAST_ATTRIBUTE
|                       **************
| This structure is for the last attribute in the TLV list.
 -------------------------------------------------------------------*/

typedef struct _Last_Attribute
{
    Per_Frame_Attribute_Header  LastHeader;

} Last_Attribute, *pLast_Attribute ;


/* ================================================================= *
 *                                                                   *
 *                              Rx Routing                           *
 *                                                                   *
 * ================================================================= */
// temp debug
#define IANS_FRAME_TYPE         0x6D88  /*  Network order is 0x886D */


/* Restore packing mode. */
#pragma pack()


#endif /* _IANS_BASE_COMM_H */


