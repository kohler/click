/*
 * nat624{cc,hh} -- element that translate an IPv6 packet to an IPv4 packet
 * 
 * Peilei Fan
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "nat624.hh"
#include "confparse.hh"
#include "error.hh"
#include "click_ip.h"
#include "click_ip6.h"
#include "ip6address.hh"

Nat624::Nat624()
{
  add_input();
  add_output();
}

Nat624::~Nat624()
{
}

void
Nat624::add_map(IPAddress ipa4, IP6Address ipa6, bool bound)
{ 
  struct Entry64 e;
  e._ip6=ipa6;
  e._ip4=ipa4;
  e._bound = bound;
  _v.push_back(e);
}


int
Nat624::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _v.clear();

  int before= errh->nerrors();
  for (int i = 0; i<conf.size(); i++) 
    {
      String arg = conf[i];
      IP6Address ipa6;
      IPAddress ipa4;
      
      for (; arg; cp_eat_space(arg)) 
	{
	  if  (cp_ip_address(arg, (unsigned char *)&ipa4, &arg))
	  { 
	   
	    if (cp_eat_space(arg) 
		&& cp_ip6_address(arg, (unsigned char *)&ipa6, &arg))
	      {
		add_map(ipa4, ipa6, 1);
	      }
	    else 
	      {
		add_map(ipa4, IP6Address("::0"), 0); 
	      }
	  }
	}
    }
  return (before ==errh->nerrors() ? 0: -1);
}


//check if there's already binding for the ipv6 address, if not, then assign the first unbinded ipv4 address to it.  If there's no ipv4 address available, return false.  

bool
Nat624::lookup(IP6Address ipa6, IPAddress &ipa4)
{ 
  int i = -1;

  for (i=0; i<_v.size(); i++) {
    if ((ipa6 == _v[i]._ip6) && (_v[i]._bound)) {
      // click_chatter("nat624::lookup -1");
      ipa4 = _v[i]._ip4;
      return (true);
    }
    else if ((ipa6 == _v[i]._ip6 ) && (_v[i]._bound==0)) {
      _v[i]._bound = 1;
      ipa4 = _v[i]._ip4;
      //click_chatter("nat624::lookup -2");
      return (true);
    }
    else if (_v[i]._bound == 0 )
      {
	_v[i]._bound =1;
	ipa4=_v[i]._ip4;
	_v[i]._ip6 = ipa6;
	//click_chatter("nat624::lookup -3");
	return (true);
      }
  }

  return (false);
}
	
//make the translation of the packet according to SIIT (RFC 2765)
Packet *
Nat624::make_translate64(IPAddress src,
			 IPAddress dst,
			 unsigned short ip6_plen, 
			 unsigned char ip6_hlim, 
			 unsigned char ip6_nxt,
			 unsigned char *a,
			 unsigned char payload_length)
{
  click_ip *ip;
  unsigned char * b;
  WritablePacket *q = Packet::make(sizeof(*ip) + ntohs(ip6_plen));
  
  if (q==0) {
    click_chatter("can not make packet!");
    assert(0);
  }

  memset(q->data(), '\0', q->length());
  ip = (click_ip *)q->data();
  b = (unsigned char * ) (ip+1);
  
  //set ipv4 header
  ip->ip_v = 4;
  ip->ip_hl =5; 
  ip->ip_tos =0;
  ip->ip_len = htons(sizeof(*ip) + ntohs(ip6_plen));
  
  ip->ip_id = htons(0);

  //set Don't Fragment flag to true, all other flags to false, 
  //set fragement offset: 0
  ip->ip_off = htons(IP_DF);
 
  ip->ip_ttl = ip6_hlim -1;

  if (ip6_nxt ==58) {
    ip->ip_p=1; 
  } else {
    ip->ip_p = ip6_nxt;
  }
 
  //set the src and dst address
  ip->ip_src = src.in_addr();
  ip->ip_dst = dst.in_addr();
 
  //copy the actual payload of packet
  memcpy(b, a, payload_length);
  //click_chatter("packet content b %x, %x, %x, %x, %x, %x", b[0], b[1], b[2], b[3], b[payload_length-2], b[payload_length-1]);

 //set the checksum
  ip->ip_sum=0;
  ip->ip_sum = in_cksum((unsigned char *)ip, ntohs(ip->ip_len));
  
  return q;	
		
}


Packet *
Nat624::simple_action(Packet *p)
{
  click_ip6 *ip6 = (click_ip6 *) p->data();
  unsigned char * start_of_p= (unsigned char *)(ip6+1);
  unsigned char pl_length = p->length() - sizeof(*ip6);
  IPAddress ipa_src;
  IPAddress ipa_dst; 

 
  Packet *q = 0;
  unsigned char ip4_dst[4];
  IP6Address ip6_dst;
  ip6_dst = IP6Address(ip6->ip6_dst);
  
  // get the corresponding ipv4 src and dst addresses
  if (ip6_dst.get_IP4Address(ip4_dst)
      && lookup(IP6Address(ip6->ip6_src), ipa_src))
    {
      ipa_dst = IPAddress(ip4_dst);
      //translate protocol according to SIIT
      q = make_translate64(ipa_src, ipa_dst, ip6->ip6_plen, ip6->ip6_hlim, ip6->ip6_nxt, start_of_p, pl_length);
    }

  p->kill();
  return(q);
}


EXPORT_ELEMENT(Nat624)

// generate Vector template instance
#include "vector.cc"
template class Vector<Nat624::Entry64>;
