/*
 * ip6address.{cc,hh} -- an IP6 address class. Useful for its hashcode()
 * method
 * Peilei Fan, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include <click/glue.hh>
#include <click/ip6address.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>

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

IP6Address::IP6Address(IPAddress ip)
{
  const unsigned char *udata = ip.data();
  //  for (int i=0; i<10; i++)
//      _addr.s6_addr[i] = 0;
//    _addr.s6_addr[10]=0xff;
//    _addr.s6_addr[11]=0xff;

  for (int i=0; i<12; i++)
      _addr.s6_addr[i] = 0;
  for (int i=0; i<4; i++)
    _addr.s6_addr[12+i] = udata[i];
}

IP6Address::IP6Address(const String &str)
{
  if (!cp_ip6_address(str, this))
    for (int i = 0; i < 4; i++)
      _addr.s6_addr32[i] = 0;
}

IP6Address
IP6Address::make_prefix(int prefix)
{
  assert(prefix >= 0 && prefix <= 128);
  static const unsigned char data[] = { 0, 128, 192, 224, 240, 248, 252, 254, 255 };
  IP6Address a;
  for (int i = 0; i < 16 && prefix > 0; i++, prefix -= 8)
    a._addr.s6_addr[i] = (prefix > 8 ? 255 : data[prefix]);
  return a;
}

int
IP6Address::mask_to_prefix_len() const
{
  int bits = 0;
  
  for (; bits < 24; bits += 8)
    if (_addr.s6_addr32[bits >> 3] != 0xFFFFFFFFU)
      break;

  // check 'swing' word
  int swing_bits = IPAddress(_addr.s6_addr32[bits >> 3]).mask_to_prefix_len();

  for (int i = (bits >> 3) + 1; i < 4; i++)
    if (_addr.s6_addr32[i] != 0)
      return -1;

  return (swing_bits >= 0 ? bits + swing_bits : -1);
}

bool
IP6Address::ip4_address(IPAddress &ip4) const
{
  if ((_addr.s6_addr16[4] == 0 && _addr.s6_addr16[5] == 0xFFFF ) 
      || ( _addr.s6_addr16[0] == 0 && _addr.s6_addr16[1] == 0 
	   && _addr.s6_addr16[2] == 0 && _addr.s6_addr16[3] == 0 
	   && _addr.s6_addr16[4] == 0 && _addr.s6_addr16[5] == 0)) 
    {
      unsigned char *d = ip4.data();
      d[0] = _addr.s6_addr[12];
      d[1] = _addr.s6_addr[13];
      d[2] = _addr.s6_addr[14];
      d[3] = _addr.s6_addr[15];
      return true;
    }
  else 
    return false;  
}

String
IP6Address::unparse() const
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
    else if ( _addr.s6_addr16[0] == 0 && _addr.s6_addr16[1] == 0 
	   && _addr.s6_addr16[2] == 0 && _addr.s6_addr16[3] == 0 
	   && _addr.s6_addr16[4] == 0 && _addr.s6_addr16[5] == 0)  {
      sprintf(buf, "::%d.%d.%d.%d", _addr.s6_addr[12], _addr.s6_addr[13],
	      _addr.s6_addr[14], _addr.s6_addr[15]);
      return String(buf);
    }
  }

  char *s = buf;
  int word, n;
  for (word = 0; word < 8 && _addr.s6_addr16[word] != 0; word++) {
    n = sprintf(s, (word ? ":%X" : "%X"), ntohs(_addr.s6_addr16[word]));
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
    n = sprintf(s, ":%X", ntohs(_addr.s6_addr16[word]));
    s += n;
  }
  *s++ = 0;
  return String(buf);
}

String
IP6Address::unparse_expanded() const
{
  char buf[48];
  sprintf(buf, "%X:%X:%X:%X:%X:%X:%X:%X",
	  ntohs(_addr.s6_addr16[0]), ntohs(_addr.s6_addr16[1]),
	  ntohs(_addr.s6_addr16[2]), ntohs(_addr.s6_addr16[3]),
	  ntohs(_addr.s6_addr16[4]), ntohs(_addr.s6_addr16[5]),
	  ntohs(_addr.s6_addr16[6]), ntohs(_addr.s6_addr16[7]));
  return String(buf);
}


//the method calculate checksums that requires a pseudoheader (e.g., tcp, udp for ipv4)
unsigned short 
in_ip4_cksum(const unsigned  saddr,
	     const unsigned  daddr,
	     unsigned short len,
	     unsigned char proto,
	     unsigned short ori_csum,
	     const unsigned char *addr,
	     unsigned short len2)
{ 
	unsigned short answer = 0;
	unsigned int csum =0;
	unsigned int carry;
	
	
	//get the sum of source and destination address
	const unsigned sa = saddr;
	csum += ntohl(sa);
 	carry = (csum < ntohl(sa));
  	csum += carry;
	
	const unsigned da = daddr;
	csum += ntohl(da);
	carry = (csum < ntohl(da));
	csum += carry;
      
	//get the sum of other fields:  packet length, protocal
	//  csum += len;
	csum += ntohs(len);
	csum += proto;
	
	//get the sum of the upper layer package
	//unsigned short nleft = len2;
	unsigned short nleft = ntohs(len2);
	const unsigned short *w = (const unsigned short *)addr;
	while (nleft > 1)  { 
	    unsigned short w2=*w++;
	    csum += ntohs(w2); 
	    nleft -=2;
	 }
  
	//mop up an odd byte, if necessary 
	if (nleft == 1) { 
	  *(unsigned char *)(&answer) = *(const unsigned char *)w ;
	  csum += ntohs(answer); 	 
	}  
	//csum -= ori_csum; //get rid of the effect of ori_csum in the calculation
	csum -= ntohs(ori_csum);
        
	// fold >=32-bit csum to 16-bits 
	while (csum>>16) {
	  csum = (csum & 0xffff) + (csum >> 16); 
	}
	  
	answer = ~csum;          // truncate to 16 bits 
	return answer;
}

// those two  methods will calculate the checksum field of ICMP6 Message.  

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
in6_fast_cksum(const struct click_in6_addr *saddr,
               const struct click_in6_addr *daddr,
               unsigned short len,
               unsigned char  proto,
               unsigned short ori_csum,
               const unsigned char *addr,
               unsigned short len2)
{ 
	unsigned short int ulen;
	unsigned short int uproto;
	unsigned short answer = 0;
	unsigned int csum =0;
	unsigned int carry;
	
	
	//get the sum of source and destination address
	for (int i=0; i<4; i++) {
	  
	  csum += ntohl(saddr->s6_addr32[i]);
	  carry = (csum < ntohl(saddr->s6_addr32[i]));
	  csum += carry;
	}

	for (int i=0; i<4; i++) {
	 
	   csum += ntohl(daddr->s6_addr32[i]);
	   carry = (csum < ntohl(daddr->s6_addr32[i]));
	   csum += carry;
	}

	//get the sum of other fields:  packet length, protocal
	ulen = ntohs(len);
	csum += ulen;

	uproto = proto;
	csum += uproto;
	
	//get the sum of the ICMP6 package
	unsigned short nleft = ntohs(len2);
	const unsigned short *w = (const unsigned short *)addr;
	while (nleft > 1)  { 
	    unsigned short w2=*w++;
	    csum += ntohs(w2); 
	    nleft -=2;
	 }
  
	 //mop up an odd byte, if necessary 
	  if (nleft == 1) { 
	    *(unsigned char *)(&answer) = *(const unsigned char *)w ;
	    csum += ntohs(answer); 	 
	  }  
	  csum -= ntohs(ori_csum); //get rid of the effect of ori_csum in the calculation
        
	  // fold >=32-bit csum to 16-bits 
	  while (csum>>16) {
	    csum = (csum & 0xffff) + (csum >> 16); 
	  }
	  
	  answer = ~csum;          // truncate to 16 bits 
	  return answer;
}


//This is the slow way for in6_cksum
unsigned short 
in6_cksum(const struct click_in6_addr *saddr,
	  const struct click_in6_addr *daddr,
	  unsigned short len, 
	  unsigned char proto,
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
	  csum += ntohs(saddr->s6_addr16[i]);
	}

	for (int i=0; i<8; i++) {
	   csum += ntohs(daddr->s6_addr16[i]);
	}

	//get the sum of other fields:  packet length, protocal
	ulen = ntohs(len);
	csum += ulen;

	uproto = proto;
	csum += uproto;
	
	//get the sum of the ICMP6 package
	unsigned short nleft = ntohs(len2);
	const unsigned short *w = (const unsigned short *)addr;
	while (nleft > 1)  { 
	    unsigned short w2=*w++;
	    csum += ntohs(w2); 
	    nleft -=2;
	 }
 
	 //mop up an odd byte, if necessary 
	  if (nleft == 1) { 
	    *(unsigned char *)(&answer) = *(const unsigned char *)w ;
	    csum += ntohs(answer); 	 
	  }  
	  csum -= ntohs(ori_csum); //get rid of the effect of ori_csum in the calculation
        
	  // fold >=32-bit csum to 16-bits 
	  while (csum>>16) {
	    csum = (csum & 0xffff) + (csum >> 16); 
	  }
	  
	  answer = ~csum;          // truncate to 16 bits 
	  return answer;
}
