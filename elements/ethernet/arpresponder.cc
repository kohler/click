/*
 * arpresponder.{cc,hh} -- element that responds to ARP queries
 * Robert Morris
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
#include "arpresponder.hh"
#include "click_ether.h"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

ARPResponder::ARPResponder()
{
  add_input();
  add_output();
}

ARPResponder::~ARPResponder()
{
}

ARPResponder *
ARPResponder::clone() const
{
  return new ARPResponder;
}

void
ARPResponder::add_map(IPAddress ipa, IPAddress mask, EtherAddress ena)
{
  struct Entry e;
  e._dst = ipa;
  e._mask = mask;
  e._ena = ena;
  _v.push_back(e);
}

int
ARPResponder::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  _v.clear();

  int before = errh->nerrors();
  for (int i = 0; i < args.size(); i++) {
    String arg = args[i];
    IPAddress ipa, mask;
    EtherAddress ena;
    bool have_ena = false;
    int first = _v.size();

    for (; arg; cp_eat_space(arg))
      if (cp_ip_address_mask(arg, ipa, mask, &arg))
	add_map(ipa, mask, EtherAddress());
      else if (cp_ethernet_address(arg, ena, &arg)) {
	if (have_ena)
	  errh->error("argument %d has more than one Ethernet address", i);
	have_ena = true;
      } else if (cp_ip_address(arg, ipa, &arg))
	add_map(ipa, IPAddress(0xFFFFFFFFU), EtherAddress());
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

Packet *
ARPResponder::make_response(u_char tha[6], /* him */
                            u_char tpa[4],
                            u_char sha[6], /* me */
                            u_char spa[4])
{
  click_ether *e;
  click_ether_arp *ea;
  Packet *q = Packet::make(sizeof(*e) + sizeof(*ea));
  if (q == 0) {
    click_chatter("in arp responder: cannot make packet!");
    assert(0);
  } 
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ea = (click_ether_arp *) (e + 1);
  memcpy(e->ether_dhost, tha, 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_ARP);
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
  ea->ea_hdr.ar_hln = 6;
  ea->ea_hdr.ar_pln = 4;
  ea->ea_hdr.ar_op = htons(ARPOP_REPLY);
  memcpy(ea->arp_tha, tha, 6);
  memcpy(ea->arp_tpa, tpa, 4);
  memcpy(ea->arp_sha, sha, 6);
  memcpy(ea->arp_spa, spa, 4);
  return q;
}

bool
ARPResponder::lookup(IPAddress a, EtherAddress &ena)
{
  int i, besti = -1;

  for(i = 0; i < _v.size(); i++){
    if((a.addr() & _v[i]._mask.addr()) == _v[i]._dst.addr()){
      if(besti == -1 || ~_v[i]._mask.addr() < ~_v[besti]._mask.addr()){
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
ARPResponder::simple_action(Packet *p)
{
  click_ether *e = (click_ether *) p->data();
  click_ether_arp *ea = (click_ether_arp *) (e + 1);
  unsigned int tpa;
  memcpy(&tpa, ea->arp_tpa, 4);
  IPAddress ipa = IPAddress(tpa);
  
  Packet *q = 0;
  if (p->length() >= sizeof(*e) + sizeof(click_ether_arp) &&
      ntohs(e->ether_type) == ETHERTYPE_ARP &&
      ntohs(ea->ea_hdr.ar_hrd) == ARPHRD_ETHER &&
      ntohs(ea->ea_hdr.ar_pro) == ETHERTYPE_IP &&
      ntohs(ea->ea_hdr.ar_op) == ARPOP_REQUEST) {
    EtherAddress ena;
    if(lookup(ipa, ena)){
      q = make_response(ea->arp_sha, ea->arp_spa,
			ena.data(), ea->arp_tpa);
    }
  } else {
    struct in_addr ina;
    memcpy(&ina, &ea->arp_tpa, 4);
  }
  
  p->kill();
  return(q);
}

EXPORT_ELEMENT(ARPResponder)


// generate Vector template instance
#include "vector.cc"
template class Vector<ARPResponder::Entry>;
