#ifndef CLICK_ETHER_H
#define CLICK_ETHER_H

/*
 * click_ether.h -- our own definitions of Ethernet and ARP headers
 * based on a file from one of the BSDs
 */

struct click_ether {
  unsigned char ether_dhost[6];
  unsigned char ether_shost[6];
  unsigned short ether_type;
};
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

/* ARP protocol HARDWARE identifiers. */
#define ARPHRD_ETHER    1       /* Ethernet 10Mbps      */

/* ARP protocol opcodes. */
#define ARPOP_REQUEST   1       /* ARP request          */
#define ARPOP_REPLY 2       /* ARP reply            */

struct click_ether_arp {
  struct click_arp ea_hdr;    /* fixed-size header */
  unsigned char arp_sha[6]; /* sender hardware address */
  unsigned char arp_spa[4]; /* sender protocol address */
  unsigned char arp_tha[6]; /* target hardware address */
  unsigned char arp_tpa[4]; /* target protocol address */
};

struct click_ether_arp6 {
  struct click_arp ea_hdr;    /* fixed-size header */
  unsigned char arp_sha[6]; /* sender hardware address */
  unsigned char arp_spa[16]; /* sender protocol address */
  unsigned char arp_tha[6]; /* target hardware address */
  unsigned char arp_tpa[16]; /* target protocol address */
};

#endif
