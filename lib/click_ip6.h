#ifndef CLICK_IP6_H
#define CLICK_IP6_H
#include "click_ip.h"

/*
 * click_ip6.h -- our own definitions of IP6 headers
 * based on a file from one of the BSDs
 */

/* IPv6 address , same as from /usr/include/netinet/in.h  */
struct click_in6_addr
  {
    union
      {
	uint8_t		u6_addr8[16];
	uint16_t	u6_addr16[8];
	uint32_t	u6_addr32[4];
#if ULONG_MAX > 0xffffffff
	uint64_t	u6_addr64[2];
#endif
      } in6_u;
#define s6_addr			in6_u.u6_addr8
#define s6_addr16		in6_u.u6_addr16
#define s6_addr32		in6_u.u6_addr32
#define s6_addr64		in6_u.u6_addr64
  };


struct click_ip6 {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned char ip6_pri:4;			/* 0   priority */
    unsigned char ip6_v:4;			/*     version == 6 */
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned char ip6_v:4;			/* 0   version == 6 */
    unsigned char ip6_pri:4;			/*     priority */
#endif
    uint8_t ip6_flow[3];             /* 1-3   flow label -follow the example from ip6.h */
    unsigned short ip6_plen;		/* 4-5   payload length */
    unsigned char ip6_nxt;		/* 6     next header */
    unsigned char ip6_hlim;     	/* 7     hop limit  */
    struct click_in6_addr ip6_src;		/* 8-23  source address */
    struct click_in6_addr ip6_dst;		/* 24-39 dest address */
};

//this method will calculate the checksum field of ICMP6 Message.  

// The checksum is the 16-bit one's complement 
// of the one's complement sum of the entire ICMPv6 message starting with the
// ICMPv6 message type field, prepended with a "pseudo-header" of IPv6 header 
// fields, as specified in [IPv6, section 8.1]. The Next Header value used in 
// the pseudo-header is 58. (NOTE: the inclusion of a pseudo-header in the 
// ICMPv6 checksum is a change from IPv4; see [RFC 2460] for the rationale 
// for this change.) 
// A "pseudo-header" include src and dst address of ipv6 header, packet length,
// protocal field (for ICMP, it is 58) field. Packet length is the 
// payloadlength from the IPv6 header, minus the length of any extension 
// header present between the IPv6 header and the upper-layer header. 

//This method first calculate the sum of the "pseudo-header" then plus the sum of the ICMP message itself (with its checksum field set to zero, though).


static unsigned short 
in6_cksum(struct click_in6_addr *saddr,
	  struct click_in6_addr *daddr,
	  unsigned short len,
	  unsigned short proto,
	  unsigned int csum,
	  unsigned char *addr,
	  unsigned short len2)
{ 
        
        //int carry;
	unsigned int ulen;
	unsigned int uproto;
	unsigned short answer = 0;
	csum =0;
	
	//set the checksum part in the message header to zero.
	for (int i=0; i<3; i++) {
	  addr[4+i]=0;
	} 
	
	//get the sum of source address
	for (int i=0; i<3; i++) {
	  csum += saddr->s6_addr32[i];
	}

	//get the sum of destination address
	for (int i=0; i<3; i++) {
	  csum += daddr->s6_addr32[i];
	}

	//get the sum of other fields:  packet length, protocal
	ulen = htonl((unsigned int) len);
	csum += ulen;
	

	uproto = htonl(proto);
	csum += uproto;
	
	//get the sum of the ICMP6 package
	unsigned short nleft = len2;
	const unsigned short *w = (const unsigned short *)addr;
	while (nleft > 1)  { 
	   csum += *w++;
	   unsigned short w2=*w++;
	   nleft -= 2;
	 }
  
	 /* mop up an odd byte, if necessary */
	 if (nleft == 1) {
	   *(unsigned char *)(&answer) = *(const unsigned char *)w ;
	   csum += answer;
	 }
  	
	/* fold >=32-bit csum to 16-bits */
	while (csum>>16) {
	  csum = (csum & 0xffff) + (csum >> 16); 
	}

	answer = ~csum;              /* truncate to 16 bits */
	click_chatter("answer: %x", answer); 
	return answer;
}

#endif
