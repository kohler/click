#ifndef CLICK_IP6_H
#define CLICK_IP6_H
#include "click_ip.h"

/*
 * click_ip6.h -- our own definitions of IP6 headers
 * based on RFC 2460
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

//these methods will calculate the checksum field of ICMP6 Message.  

// The checksum is the 16-bit one's complement 
// of the one's complement sum of the entire ICMPv6 message starting with the
// ICMPv6 message type field, prepended with a "pseudo-header" of IPv6 header 
// fields, as specified in [IPv6, section 8.1]. The Next Header value used in 
// the pseudo-header is 58 (i.e. 0x3a). (NOTE: the inclusion of a pseudo-header in the 
// ICMPv6 checksum is a change from IPv4; see [RFC 2460] for the rationale 
// for this change.) 
// A "pseudo-header" include src and dst address of ipv6 header, packet length,
// protocal field (for ICMP, it is 58) field. Packet length is the 
// payloadlength from the IPv6 header, minus the length of any extension 
// header present between the IPv6 header and the upper-layer header. 

// The following methods only differ at how it deal with ip6 address, i.e. add 32 bit 
// a time or 16 bits a time.

static unsigned short 
in6_fast_cksum(struct click_in6_addr *saddr,
	  struct click_in6_addr *daddr,
	  unsigned short len,
	  unsigned short proto,
	  unsigned short ori_csum,
	  unsigned char *addr,
	  unsigned short len2)
{ 
	unsigned short int ulen;
	unsigned short int uproto;
	unsigned short answer = 0;
	unsigned int csum =0;
	unsigned int carry;
	
	
	//get the sum of source and destination address
	for (int i=0; i<4; i++) {
	  //click_chatter("src address: %8x", ntohl(saddr->s6_addr32[i]));
	  csum += ntohl(saddr->s6_addr32[i]);
	  carry = (csum < ntohl(saddr->s6_addr32[i]));
	  csum += carry;
	}

	for (int i=0; i<4; i++) {
	  //click_chatter("dst address: %8x", ntohl(daddr->s6_addr32[i])); 
	   csum += ntohl(daddr->s6_addr32[i]);
	   carry = (csum < ntohl(daddr->s6_addr32[i]));
	   csum += carry;
	}

	//get the sum of other fields:  packet length, protocal
	ulen = ntohs(len);
	//click_chatter("packet length: %4x", ulen);
	csum += ulen;

	uproto = proto;
	//click_chatter(" protocol: %4x", uproto);
	csum += uproto;
	
	//get the sum of the ICMP6 package
	unsigned short nleft = len2;
	const unsigned short *w = (const unsigned short *)addr;
	while (nleft > 1)  { 
	    unsigned short w2=*w++;
	    //click_chatter(" packet content: %2x", ntohs(w2));
	    csum += ntohs(w2); 
	    nleft -=2;
	 }
  
	 //mop up an odd byte, if necessary 
	  if (nleft == 1) { 
	    *(unsigned char *)(&answer) = *(const unsigned char *)w ;
	    //click_chatter(" last packet content: %2x", ntohs(answer));
	    csum += ntohs(answer); 	 
	  }  
	  csum -= ntohs(ori_csum); //get rid of the effect of ori_csum in the calculation
        
	  // fold >=32-bit csum to 16-bits 
	  while (csum>>16) {
	    csum = (csum & 0xffff) + (csum >> 16); 
	  }
	  
	  answer = ~csum;          // truncate to 16 bits 
	  //click_chatter("answer: %x", answer); 
	  return answer;
}


//This is the slow way for in6_cksum
static unsigned short 
in6_cksum(struct click_in6_addr *saddr,
	  struct click_in6_addr *daddr,
	  unsigned short len,
	  unsigned short proto,
	  unsigned short ori_csum,
	  unsigned char *addr,
	  unsigned short len2)
{ 
	unsigned short int ulen;
	unsigned short int uproto;
	unsigned short answer = 0;
	unsigned int csum =0;
	
	
	//get the sum of source and destination address
	for (int i=0; i<8; i++) {
	  //click_chatter("src address: %4x", ntohs(saddr->s6_addr16[i]));
	  csum += ntohs(saddr->s6_addr16[i]);
	}

	for (int i=0; i<8; i++) {
	  //click_chatter("dst address: %4x", ntohs(daddr->s6_addr16[i]));
	   csum += ntohs(daddr->s6_addr16[i]);
	}

	//get the sum of other fields:  packet length, protocal
	ulen = ntohs(len);
	//click_chatter("packet length: %4x", ulen);
	csum += ulen;

	uproto = proto;
	//click_chatter(" protocol: %4x", uproto);
	csum += uproto;
	
	//get the sum of the ICMP6 package
	unsigned short nleft = len2;
	const unsigned short *w = (const unsigned short *)addr;
	while (nleft > 1)  { 
	    unsigned short w2=*w++;
	    //click_chatter(" packet content: %2x", ntohs(w2));
	    csum += ntohs(w2); 
	    nleft -=2;
	 }
  
	 //mop up an odd byte, if necessary 
	  if (nleft == 1) { 
	    *(unsigned char *)(&answer) = *(const unsigned char *)w ;
	    //click_chatter(" last packet content: %2x", ntohs(answer));
	    csum += ntohs(answer); 	 
	  }  
	  csum -= ntohs(ori_csum); //get rid of the effect of ori_csum in the calculation
        
	  // fold >=32-bit csum to 16-bits 
	  while (csum>>16) {
	    csum = (csum & 0xffff) + (csum >> 16); 
	  }
	  
	  answer = ~csum;          // truncate to 16 bits 
	  //click_chatter("answer: %x", answer); 
	  return answer;
}

#endif
