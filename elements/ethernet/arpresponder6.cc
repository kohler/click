/*
 * arpresponder6.{cc,hh} -- element that responds to Neighborhood Solitation Msg
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
#include "arpresponder6.hh"
#include "click_ether.h"
#include "click_ip6.h"
#include "etheraddress.hh"
#include "ip6address.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"


ARPResponder6::ARPResponder6()
{
  add_input();
  add_output();
}

ARPResponder6::~ARPResponder6()
{
}

ARPResponder6 *
ARPResponder6::clone() const
{
  return new ARPResponder6;
}

void
ARPResponder6::add_map(IP6Address ipa, IP6Address mask, EtherAddress ena)
{
  struct Entry e;

  e._dst = ipa;
  e._mask = mask;
  e._ena = ena;
  _v.push_back(e);
}

int
ARPResponder6::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _v.clear();
  
  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    IP6Address ipa, mask;
    EtherAddress ena;
    bool have_ena = false;
    int first = _v.size();

    Vector<String> words;
    cp_spacevec(conf[i], words);
    
    for (int j = 0; j < words.size(); j++)
      if (cp_ip6_prefix(words[j], (unsigned char *)&ipa, (unsigned char *)&mask, true, this))
	add_map(ipa, mask, EtherAddress());
      else if (cp_ip6_address(words[j], ipa, this))
	add_map(ipa, IP6Address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"), EtherAddress());
      else if (cp_ethernet_address(words[j], ena, this)) {	
	if (have_ena)
	  errh->error("argument %d has more than one Ethernet address", i);
	have_ena = true;
      } else {
	errh->error("argument %d should be `IP6/MASK ETHADDR'", i);
	j = words.size();
      }

	
    //    if (cp_ethernet_address(words[j], ena)) {
//  	if (have_ena)
//  	  errh->error("argument %d has more than one Ethernet address", i);
//  	have_ena = true;
//        } else if (cp_ip6_address(words[j], ipa)) {
//  	add_map(ipa, IP6Address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"), EtherAddress());
//        } else {
//  	errh->error("argument %d should be `IP6ADDR MASK ETHADDR'", i);
//  	j = words.size();
//        } 

    if (first == _v.size())
      errh->error("argument %d had no IP6 address and masks", i);
    for (int j = first; j < _v.size(); j++)
      _v[j]._ena = ena;
  }

  return (before == errh->nerrors() ? 0 : -1);
}


Packet *
ARPResponder6::make_response(u_char dha[6],   /*  des eth address */
			     u_char sha[6],   /*  src eth address */
			     u_char dpa[16],  /*  dst IP6 address */
			     u_char spa[16],  /*  src IP6 address */
			     u_char tpa[16],  /*  target IP6 address */
			     u_char tha[6])   /*  target eth address */

{
  click_ether *e;
  click_ip6 *ip6;
  click_arp6resp *ea;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*ip6)+ sizeof(*ea));
  if (q == 0) {
    click_chatter("in arp responder: cannot make packet!");
    assert(0);
  } 
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ip6=(click_ip6 *) (e+1);
  ea = (click_arp6resp *) (ip6 + 1);
  
  //set ethernet header
  memcpy(e->ether_dhost, dha, 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_IP6);

  //set ip6 header
  ip6->ip6_v=6;
  ip6->ip6_pri=0;
  ip6->ip6_flow[0]=0;
  ip6->ip6_flow[1]=0;
  ip6->ip6_flow[2]=0;
  ip6->ip6_plen=htons(sizeof(click_arp6resp));
  ip6->ip6_nxt=0x3a; //i.e. protocal: icmp6 message
  ip6->ip6_hlim=0xff; //indicate no router has processed it
  ip6->ip6_src = IP6Address(spa);
  ip6->ip6_dst = IP6Address(dpa);

 
  //set Neighborhood Solicitation Validation Message
  ea->type = 0x88; 
  ea->code =0;
 
  ea->flags = 0x40; //  this is the same as setting the following
                    //  ea->sender_is_router = 0;
                    //  ea->solicited =1;
                    //  ea->override = 0;
  
  for (int i=0; i<3; i++) {
    ea->reserved[i] = 0;
  }
  
  memcpy(ea->arp_tpa, tpa, 16);
  ea->option_type = 0x2;
  ea->option_length = 0x1;
  memcpy(ea->arp_tha, tha, 6);
  ea->checksum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, 0, (unsigned char *)(ip6+1), sizeof(click_arp6resp)));
  
  //click_chatter("set checksum of neigh adv messag as : %x", ntohs(ea->checksum));
  return q;
}

Packet *
ARPResponder6::make_response2(u_char dha[6],   /*  des eth address */
			     u_char sha[6],   /*  src eth address */
			     u_char dpa[16],  /*  dst IP6 address */
			     u_char spa[16],  /*  src IP6 address */
			     u_char tpa[16])  /*  target IP6 address */
			     
{
  click_ether *e;
  click_ip6 *ip6;
  click_arp6resp2 *ea;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*ip6)+ sizeof(*ea));
  if (q == 0) {
    click_chatter("in arp responder: cannot make packet!");
    assert(0);
  } 
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ip6=(click_ip6 *) (e+1);
  ea = (click_arp6resp2 *) (ip6 + 1);
  
  //set ethernet header
  memcpy(e->ether_dhost, dha, 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_IP6);

  //set ip6 header
  ip6->ip6_v=6;
  ip6->ip6_pri=0;
  ip6->ip6_flow[0]=0;
  ip6->ip6_flow[1]=0;
  ip6->ip6_flow[2]=0;
  ip6->ip6_plen=htons(sizeof(click_arp6resp2));
  ip6->ip6_nxt=0x3a; //i.e. protocal: icmp6 message
  ip6->ip6_hlim=0xff; //indicate no router has processed it
  ip6->ip6_src = IP6Address(spa);
  ip6->ip6_dst = IP6Address(dpa);

 
  //set Neighborhood Solicitation Validation Message
  ea->type = 0x88; 
  ea->code =0;
 
  ea->flags = 0x40; //  this is the same as setting the following
                    //  ea->sender_is_router = 0;
                    //  ea->solicited =1;
                    //  ea->override = 0;
  
  for (int i=0; i<3; i++) {
    ea->reserved[i] = 0;
  }
  
  memcpy(ea->arp_tpa, tpa, 16);
 
  ea->checksum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, 0, (unsigned char *)(ip6+1), sizeof(click_arp6resp2)));
  
  //click_chatter("set checksum of neigh adv messag as : %x", ntohs(ea->checksum));
  return q;
}


bool
ARPResponder6::lookup(IP6Address a, EtherAddress &ena)
{
  int i, besti = -1;

  for(i = 0; i < _v.size(); i++) {
    if ((a & _v[i]._mask) == _v[i]._dst)
      {
      if(besti == -1 || ~_v[i]._mask < ~_v[besti]._mask ){
        besti = i;
      }
    }
  }

  if(besti == -1){
    return(false);
  } else {
    ena = _v[besti]._ena;
    return(true);
  }
}

Packet *
ARPResponder6::simple_action(Packet *p)
{
   click_ether *e = (click_ether *) p->data();
   click_ip6 *ip6 = (click_ip6 *) (e + 1);
   click_arp6req *ea=(click_arp6req *) (ip6 + 1);
   unsigned char tpa[16];
   unsigned char spa[16];
   unsigned char dpa[16];
   memcpy(&tpa, ea->arp_tpa, 16);
   memcpy(&dpa, IP6Address(ip6->ip6_src).data(), 16);
   IP6Address ipa = IP6Address(tpa);

   //check see if the packet is corrupted by recalculate its checksum
   unsigned short int csum2 = in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, ea->checksum, (unsigned char *)(ip6+1), sizeof(click_arp6req));
   //click_chatter(" recalculated its checksum is %x", csum2);

   Packet *q = 0;

  if (p->length() >= sizeof(*e) + sizeof(click_ip6) + sizeof(click_arp6req) &&
      ntohs(e->ether_type) == ETHERTYPE_IP6 &&
      ip6->ip6_hlim ==0xff &&
      ea->type == NEIGH_SOLI &&
      ea->code == 0 &&
      csum2 == ntohs(ea->checksum))    
    {
      EtherAddress ena;
      unsigned char host_ether[6];
      if(lookup(ipa, ena)) 
	{
	  memcpy(&spa, ipa.data(), 16); 
	  memcpy(&host_ether, ena.data(),6);
	  if ((e->ether_dhost[0]==0x33) && (e->ether_dhost[1]==0x33)) {
	    click_chatter("this is multicast neigh. solitation msg");
	  //use the finded ip6address as its source ip6 address in the header in neighborhood advertisement message packet
	  
	    q = make_response(e->ether_shost, host_ether, dpa, spa, tpa, host_ether);
	  }
	  else {
	    q = make_response2(e->ether_shost, host_ether, dpa, spa, tpa); 
	  }
	}
    } 
  
  else 
    {
      click_in6_addr ina;
      memcpy(&ina, &ea->arp_tpa, 16);
    }
  
  p->kill();
  return(q);
}

ELEMENT_REQUIRES(ip6)
EXPORT_ELEMENT(ARPResponder6)

// generate Vector template instance
#include "vector.cc"
template class Vector<ARPResponder6::Entry>;




