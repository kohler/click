/*
 * arpresponder6.{cc,hh} -- element that responds to ARP queries
 * Robert Morris
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
#include "arpresponder6.hh"
#include "click_ether.h"
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
    String arg = conf[i];
    IP6Address ipa, mask;
    EtherAddress ena;
    bool have_ena = false;
    int first = _v.size();

    for (; arg; cp_eat_space(arg))
      if (cp_ip6_address(arg, (unsigned char *)&ipa, &arg)
	&& cp_eat_space(arg)
        && cp_ip6_address(arg, (unsigned char *)&mask, &arg))
	//(cp_ip_address_mask(arg, ipa, mask, &arg)) 
	add_map(ipa, mask, EtherAddress());
      else if (cp_ethernet_address(arg, ena, &arg)) {
	if (have_ena)
	  errh->error("argument %d has more than one Ethernet address", i);
	have_ena = true;
      } else if (cp_ip6_address(arg, ipa, &arg))
	{
	  //IP6Address *myip6 = new IP6Address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
	  add_map(ipa, IP6Address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"), EtherAddress());
	}
      else {
	errh->error("argument %d should be `IPADDR MASK ETHADDR'", i);
	arg = "";
      }

    if (first == _v.size())
      errh->error("argument %d had no IP address and masks", i);
    for (int j = first; j < _v.size(); j++)
      _v[j]._ena = ena;
  }

  return (before == errh->nerrors() ? 0 : -1);
}

   //   if (cp_ip6_address(arg, (unsigned char *)&ipa, &arg)
//  	&& cp_eat_space(arg)
//          && cp_ip6_address(arg, (unsigned char *)&mask, &arg)
//          && cp_eat_space(arg)
//  	&& cp_ethernet_address(arg, ena))
//        {
//  	set_map(ipa, mask, ena);
//  	click_chatter("\n ARPResponder: ip6addr \n");
//  	ipa.print(); 
//  	click_chatter("\n ARPResponder: mask \n");
//  	mask.print();
//  	click_chatter("\n ######### ARPResponder6 configuration successful ! \n");
//        }
//      else {
//        errh->error("ARPResponder6 configuration expected ip6, mask, and ether addr");
//        return -1;
//      }
//    }
  
//    return 0;
//  } 


Packet *
ARPResponder6::make_response(u_char tha[6], /* him */
                            u_char tpa[16],
                            u_char sha[6], /* me */
                            u_char spa[16])
{
  click_ether *e;
  click_ether_arp6 *ea;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*ea));
  if (q == 0) {
    click_chatter("in arp responder: cannot make packet!");
    assert(0);
  } 
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ea = (click_ether_arp6 *) (e + 1);
  memcpy(e->ether_dhost, tha, 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_ARP);
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP6);
  ea->ea_hdr.ar_hln = 6;
  ea->ea_hdr.ar_pln = 16;
  ea->ea_hdr.ar_op = htons(ARPOP_REPLY);
  memcpy(ea->arp_tha, tha, 6);
  memcpy(ea->arp_tpa, tpa, 16);
  memcpy(ea->arp_sha, sha, 6);
  memcpy(ea->arp_spa, spa, 16);
  return q;
}

//  {
//    struct ether_header *e;
//    struct ether_arp6 *ea;
//    Packet *q = Packet::make(sizeof(*e) + sizeof(*ea));
//    memset(q->data(), '\0', q->length());
//    e = (struct ether_header *) q->data();
//    ea = (struct ether_arp6 *) (e + 1);
//    memcpy(e->ether_dhost, tha, 6);
//    memcpy(e->ether_shost, sha, 6);
//    e->ether_type = htons(ETHERTYPE_ARP);
//    ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
//    ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
//    ea->ea_hdr.ar_hln = 6;
//    ea->ea_hdr.ar_pln = 4;
//    ea->ea_hdr.ar_op = htons(ARPOP_REPLY);
//    memcpy(ea->arp_tha, tha, 6);
//    memcpy(ea->arp_tpa, tpa, 16);
//    memcpy(ea->arp_sha, sha, 6);
//    memcpy(ea->arp_spa, spa, 16);
//    return q;
//  } 

bool
ARPResponder6::lookup(IP6Address a, EtherAddress &ena)
{
  int i, besti = -1;

  for(i = 0; i < _v.size(); i++){
    /* if(((a.saddr().add[0] & _v[i]._mask.saddr().add[0]) == _v[i]._dst.saddr().add[0]) &&
       ((a.saddr().add[1] & _v[i]._mask.saddr().add[1]) == _v[i]._dst.saddr().add[1]) && 
       ((a.saddr().add[2] & _v[i]._mask.saddr().add[2]) == _v[i]._dst.saddr().add[2]) &&
       ((a.saddr().add[3] & _v[i]._mask.saddr().add[3]) == _v[i]._dst.saddr().add[3]))
    */
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
  click_ether_arp6 *ea = (click_ether_arp6 *) (e + 1);
  //  struct ether_header *e = (struct ether_header *) p->data();
//    struct ether_arp6 *ea = (struct ether_arp6 *) (e + 1);
  unsigned int tpa;
  memcpy(&tpa, ea->arp_tpa, 16);
  IP6Address ipa = IP6Address((unsigned char *)tpa);
  
  Packet *q = 0;
  if (p->length() >= sizeof(*e) + sizeof(click_ether_arp6) &&
      ntohs(e->ether_type) == ETHERTYPE_ARP &&
      ntohs(ea->ea_hdr.ar_hrd) == ARPHRD_ETHER &&
      ntohs(ea->ea_hdr.ar_pro) == ETHERTYPE_IP6 &&
      ntohs(ea->ea_hdr.ar_op) == ARPOP_REQUEST) {
    EtherAddress ena;
    if(lookup(ipa, ena)){
      q = make_response(ea->arp_sha, ea->arp_spa,
			ena.data(), ea->arp_tpa);
    }
  } else {
    struct in_addr ina;
    memcpy(&ina, &ea->arp_tpa, 16);
  }
  
  p->kill();
  return(q);
}

EXPORT_ELEMENT(ARPResponder6)
ELEMENT_REQUIRES(false)

// generate Vector template instance
#include "vector.cc"
template class Vector<ARPResponder6::Entry>;
