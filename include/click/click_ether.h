/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICK_ETHER_H
#define CLICK_ETHER_H

/*
 * click_ether.h -- our own definitions of Ethernet and ARP headers
 * based on a file from one of the BSDs
 */

#ifndef __GNUC__
# error "GNU C's __attribute__((packed)) extension required"
#endif
  
struct click_ether {
    uint8_t	ether_dhost[6];
    uint8_t	ether_shost[6];
    uint16_t	ether_type;
} __attribute__ ((packed));
#define ETHERTYPE_IP	0x0800
#define ETHERTYPE_IP6	0x86DD
#define ETHERTYPE_ARP	0x0806

#define ETHERTYPE_GRID	0x7fff	/* wvlan_cs driver won't transmit frames with high bit of protocol number set */

struct click_arp {
    uint16_t	ar_hrd;		/* Format of hardware address.  */
    uint16_t	ar_pro;		/* Format of protocol address.  */
    uint8_t	ar_hln;		/* Length of hardware address.  */
    uint8_t	ar_pln;		/* Length of protocol address.  */
    uint16_t	ar_op;		/* ARP opcode (command).  */
};

/* ARP protocol HARDWARE identifiers. */
#define ARPHRD_ETHER    1       /* Ethernet 10Mbps      */

/* ARP protocol opcodes. */
#define ARPOP_REQUEST   1       /* ARP request          */
#define ARPOP_REPLY 2           /* ARP reply            */

struct click_ether_arp {
    struct click_arp ea_hdr;	/* fixed-size header */
    uint8_t	arp_sha[6];	/* sender hardware address */
    uint8_t	arp_spa[4];	/* sender protocol address */
    uint8_t	arp_tha[6];	/* target hardware address */
    uint8_t	arp_tpa[4];	/* target protocol address */
};


#define ND_SOL 0x0087       /* Neighborhood Solicitation Message Type */
#define ND_ADV 0x0088       /* Neighborhood Advertisement Message Type */

/* define structure of Neighborhood Solicitation Message */
struct click_nd_sol {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t reserved;
    uint8_t nd_tpa[16];
    uint8_t option_type;   /*option type: 1 (source link-layer add) */
    uint8_t option_length; /*option length: 1 (in units of 8 octets) */
    uint8_t nd_sha[6];    /*source link-layer address */
};

//define structure of Neighborhood Advertisement Message -reply to multicast neighborhood solitation message
struct click_nd_adv {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint8_t flags; /* bit 1: sender_is_router
                      bit 2: solicited
		      bit 3: override
		      all other bits should be zero */
    uint8_t reserved[3];
    uint8_t nd_tpa[16];
    uint8_t option_type;    /* option type: 2 (target link-layer add) */
    uint8_t option_length;  /* option length: 1 (in units of 8 octets) */
    uint8_t nd_tha[6];     /* source link-layer address */
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
