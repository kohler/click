/*
 * ip6address.{cc,hh} -- an IP6 address class. Useful for its hashcode()
 * method
 * Robert Morris / John Jannotti / Peilei Fan
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ip6address.hh"
#include "ipaddress.hh"
#include "confparse.hh"

IP6Address::IP6Address()
{
  for (int i = 0; i < 4; i++)
    _addr.s6_addr32[i] = 0;
}

IP6Address::IP6Address(const unsigned char *data)
{
  const unsigned *udata = reinterpret_cast<const unsigned *>(data);
  for (int i = 0; i < 4; i++)
    _addr.s6_addr32[i] = udata[i];
}

IP6Address::IP6Address(const String &str)
{
  if (!cp_ip6_address(str, *this))
    for (int i = 0; i < 4; i++)
      _addr.s6_addr32[i] = 0;
}

bool
IP6Address::get_ip4address(unsigned char ip4[4]) const
{
  if (_addr.s6_addr16[4] == 0 && _addr.s6_addr16[5] == 0xFFFF) {
    ip4[0] = _addr.s6_addr[12];
    ip4[1] = _addr.s6_addr[13];
    ip4[2] = _addr.s6_addr[14];
    ip4[3] = _addr.s6_addr[15];
    return true;
  }
 else 
   return false;
    
}

String
IP6Address::s() const
{
  char buf[48];
  
  // do some work to print the address well
  if (_addr.s6_addr32[0] == 0 && _addr.s6_addr32[1] == 0) {
    if (_addr.s6_addr32[2] == 0 && _addr.s6_addr32[3] == 0)
      return "::";		// empty address
    else if (_addr.s6_addr32[2] == 0) {
      sprintf(buf, "::%d.%d.%d.%d", _addr.s6_addr[12], _addr.s6_addr[13],
	      _addr.s6_addr[14], _addr.s6_addr[15]);
      return String(buf);
    } else if (_addr.s6_addr16[4] == 0 && _addr.s6_addr16[5] == 0xFFFF) {
      sprintf(buf, "::FFFF:%d.%d.%d.%d", _addr.s6_addr[12], _addr.s6_addr[13],
	      _addr.s6_addr[14], _addr.s6_addr[15]);
      return String(buf);
    }
  }

  char *s = buf;
  int word, n;
  for (word = 0; word < 8 && _addr.s6_addr16[word] != 0; word++) {
    sprintf(s, (word ? ":%X%n" : "%X%n"), ntohs(_addr.s6_addr16[word]), &n);
    s += n;
  }
  if (word == 0 || (word < 7 && _addr.s6_addr16[word+1] == 0)) {
    *s++ = ':';
    while (word < 8 && _addr.s6_addr16[word] == 0)
      word++;
    if (word == 8)
      *s++ = ':';
  }
  for (; word < 8; word++) {
    sprintf(s, ":%X%n", ntohs(_addr.s6_addr16[word]), &n);
    s += n;
  }
  *s++ = 0;
  return String(buf);
}

String
IP6Address::full_s() const
{
  char buf[48];
  sprintf(buf, "%X:%X:%X:%X:%X:%X:%X:%X",
	  ntohs(_addr.s6_addr16[0]), ntohs(_addr.s6_addr16[1]),
	  ntohs(_addr.s6_addr16[2]), ntohs(_addr.s6_addr16[3]),
	  ntohs(_addr.s6_addr16[4]), ntohs(_addr.s6_addr16[5]),
	  ntohs(_addr.s6_addr16[6]), ntohs(_addr.s6_addr16[7]));
  return String(buf);
}




// these methods will calculate the checksum field of ICMP6 Message.  

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

unsigned short 
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
unsigned short 
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
