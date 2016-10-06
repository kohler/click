/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICKNET_ETHER_H
#define CLICKNET_ETHER_H

#include "ip.h"

/*
 * <clicknet/ether.h> -- Ethernet and ARP headers, based on one of the BSDs.
 *
 * Relevant RFCs include:
 *   RFC826	Ethernet Address Resolution Protocol: or, Converting Network
 *		Protocol Addresses to 48.bit Ethernet Address for Transmission
 *		on Ethernet Hardware
 *   RFC894	Standard for the transmission of IP datagrams over Ethernet
 *		networks
 * Also see the relevant IEEE 802 standards.
 */

#define ETHER_ADDR_LEN 6

struct click_ether {
    uint8_t	ether_dhost[ETHER_ADDR_LEN];	/* 0-5   Ethernet destination address */
    uint8_t	ether_shost[ETHER_ADDR_LEN];	/* 6-11  Ethernet source address      */
    uint16_t	ether_type;			/* 12-13 Ethernet protocol	      */
} CLICK_SIZE_PACKED_ATTRIBUTE;

#define ETHERTYPE_IP		0x0800
#define ETHERTYPE_ARP		0x0806
#define ETHERTYPE_TRAIL		0x1000
#define ETHERTYPE_8021Q		0x8100
#define ETHERTYPE_IP6		0x86DD
#define ETHERTYPE_MACCONTROL	0x8808
#define ETHERTYPE_PPPOE_DISC	0x8863
#define ETHERTYPE_PPPOE_SESSION	0x8864
#define ETHERTYPE_GRID		0x7fff	/* wvlan_cs driver won't transmit frames with high bit of protocol number set */

struct click_arp {		/* Offsets relative to ARP (Ethernet) header */
    uint16_t	ar_hrd;		/* 0-1 (14-15)  hardware address format      */
#define ARPHRD_ETHER	1	/*		  Ethernet 10Mbps	     */
#define ARPHRD_IEEE802	6	/*		  token ring     	     */
#define ARPHRD_ARCNET	7	/*		  Arcnet         	     */
#define ARPHRD_FRELAY	15	/*		  frame relay    	     */
#define ARPHRD_STRIP	23	/*		  Ricochet Starmode Radio    */
#define ARPHRD_IEEE1394	24	/*		  IEEE 1394 (FireWire)	     */
#define ARPHRD_80211    801	/*		  IEEE 802.11 (wifi)	     */
    uint16_t	ar_pro;		/* 2-3 (16-17)  protocol address format      */
    uint8_t	ar_hln;		/* 4   (18)     hardware address length      */
    uint8_t	ar_pln;		/* 5   (19)     protocol address length      */
    uint16_t	ar_op;		/* 6-7 (20-21)  opcode (command)	     */
#define ARPOP_REQUEST	1	/*		  ARP request		     */
#define ARPOP_REPLY	2	/*		  ARP reply		     */
#define ARPOP_REVREQUEST 3	/*		  reverse request: hw->proto */
#define ARPOP_REVREPLY	4	/*		  reverse reply		     */
#define ARPOP_INVREQUEST 8	/*		  peer identification req    */
#define ARPOP_INVREPLY	9	/*		  peer identification reply  */
};

struct click_ether_arp {
    struct click_arp ea_hdr;			/* 0-7   (14-21)  fixed-size ARP header	     */
    uint8_t	arp_sha[ETHER_ADDR_LEN];	/* 8-13  (22-27)  sender hardware address    */
    uint8_t	arp_spa[IP_ADDR_LEN];		/* 14-17 (28-31)  sender protocol address    */
    uint8_t	arp_tha[ETHER_ADDR_LEN];	/* 18-23 (32-37)  target hardware address    */
    uint8_t	arp_tpa[IP_ADDR_LEN];		/* 24-27 (38-41)  target protocol address    */
};


/* Ethernet with VLAN (802.1q) */

struct click_ether_vlan {
    uint8_t	ether_dhost[ETHER_ADDR_LEN];	/* 0-5   Ethernet source address      */
    uint8_t	ether_shost[ETHER_ADDR_LEN];	/* 6-11  Ethernet destination address */
    uint16_t	ether_vlan_proto;		/* 12-13 == ETHERTYPE_8021Q	      */
    uint16_t	ether_vlan_tci;			/* 14-15 tag control information      */
    uint16_t	ether_vlan_encap_proto;		/* 16-17 Ethernet protocol	      */
} CLICK_SIZE_PACKED_ATTRIBUTE;


/* Ethernet MAC control (802.3) */

struct click_ether_macctl {
    uint16_t	ether_macctl_opcode;
    uint16_t	ether_macctl_param;
    uint8_t	ether_macctl_reserved[42];
};

#define ETHER_MACCTL_OP_PAUSE	0x0001


#define ND_SOL 0x0087       /* Neighborhood Solicitation Message Type */
#define ND_ADV 0x0088       /* Neighborhood Advertisement Message Type */

/* define structure of Neighborhood Solicitation Message */
struct click_nd_sol {
    uint8_t	type;
    uint8_t	code;
    uint16_t	checksum;
    uint32_t	reserved;
    uint8_t	nd_tpa[16];
    uint8_t	option_type;		/*option type: 1 (source link-layer add) */
    uint8_t	option_length;		/*option length: 1 (in units of 8 octets) */
    uint8_t	nd_sha[ETHER_ADDR_LEN];	/*source link-layer address */
};

/* define structure of Neighborhood Advertisement Message -reply to multicast neighborhood solitation message */
struct click_nd_adv {
    uint8_t	type;
    uint8_t	code;
    uint16_t	checksum;
    uint8_t	flags; /* bit 1: sender_is_router
			  bit 2: solicited
			  bit 3: override
			  all other bits should be zero */
    uint8_t	reserved[3];
    uint8_t	nd_tpa[16];
    uint8_t	option_type;		/* option type: 2 (target link-layer add) */
    uint8_t	option_length;		/* option length: 1 (in units of 8 octets) */
    uint8_t	nd_tha[ETHER_ADDR_LEN];	/* source link-layer address */
};


/* define structure of Neighborhood Advertisement Message - reply to unicast neighborhood solitation message */
struct click_nd_adv2 {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint8_t flags; /* bit 1: sender_is_router
		      bit 2: solicited
		      bit 3: override
		      all other bits should be zero */
    uint8_t reserved[3];
    uint8_t nd_tpa[16];
};

#endif
