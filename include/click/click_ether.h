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
  unsigned char ether_dhost[6];
  unsigned char ether_shost[6];
  unsigned short ether_type;
} __attribute__ ((packed));
#define ETHERTYPE_IP	0x0800
#define ETHERTYPE_IP6	0x86DD
#define ETHERTYPE_ARP	0x0806

#define ETHERTYPE_GRID	0x7fff // wvlan_cs driver won't transmit frames with high bit of protocol number set

struct click_arp {
  unsigned short int ar_hrd;  /* Format of hardware address.  */
  unsigned short int ar_pro;  /* Format of protocol address.  */
  unsigned char ar_hln;       /* Length of hardware address.  */
  unsigned char ar_pln;       /* Length of protocol address.  */
  unsigned short int ar_op;   /* ARP opcode (command).  */
};

//define structure of Neighborhood Solicitation Message
struct click_nd_sol {
  unsigned char type;
  unsigned char code;
  unsigned short int checksum;
  unsigned int reserved;
  unsigned char nd_tpa[16];
  unsigned char option_type;   //option type: 1 (source link-layer add)
  unsigned char option_length; //option length: 1 (in units of 8 octets)
  unsigned char nd_sha[6];    //source link-layer address
};

//define structure of Neighborhood Advertisement Message -reply to multicast neighborhood solitation message
struct click_nd_adv {
  unsigned char type;
  unsigned char code;
  unsigned short int checksum;
  unsigned char flags; // bit 1: sender_is_router
                       // bit 2: solicited
                       // bit 3: override
                       // all other bits should be zero
  unsigned char reserved[3];
  unsigned char nd_tpa[16];
  unsigned char option_type;    //option type: 2 (target link-layer add)
  unsigned char option_length;  //option length: 1 (in units of 8 octets)    
  unsigned char nd_tha[6];     //source link-layer address
};


//define structure of Neighborhood Advertisement Message - reply to unicast neighborhood solitation message
struct click_nd_adv2 {
  unsigned char type;
  unsigned char code;
  unsigned short int checksum;
  unsigned char flags; // bit 1: sender_is_router
                       // bit 2: solicited
                       // bit 3: override
                       // all other bits should be zero
  unsigned char reserved[3];
  unsigned char nd_tpa[16];
};



/* ARP protocol HARDWARE identifiers. */
#define ARPHRD_ETHER    1       /* Ethernet 10Mbps      */

/* ARP protocol opcodes. */
#define ARPOP_REQUEST   1       /* ARP request          */
#define ARPOP_REPLY 2           /* ARP reply            */
#define ND_SOL 0x0087       /* Neighborhood Solicitation Message Type */
#define ND_ADV 0x0088       /* Neighborhood Advertisement Message Type */

struct click_ether_arp {
  struct click_arp ea_hdr;    /* fixed-size header */
  unsigned char arp_sha[6]; /* sender hardware address */
  unsigned char arp_spa[4]; /* sender protocol address */
  unsigned char arp_tha[6]; /* target hardware address */
  unsigned char arp_tpa[4]; /* target protocol address */
};

#endif
