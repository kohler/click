/*
 * arpresponder.{cc,hh} -- element that responds to ARP queries
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "arpresponder.hh"
#include <click/click_ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

ARPResponder::ARPResponder()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

ARPResponder::~ARPResponder()
{
  MOD_DEC_USE_COUNT;
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
ARPResponder::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _v.clear();

  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    IPAddress ipa, mask;
    EtherAddress ena;
    bool have_ena = false;
    int first = _v.size();

    Vector<String> words;
    cp_spacevec(conf[i], words);
    
    for (int j = 0; j < words.size(); j++)
      if (cp_ip_prefix(words[j], &ipa, &mask, this))
	add_map(ipa, mask, EtherAddress());
      else if (cp_ip_address(words[j], &ipa, this))
	add_map(ipa, IPAddress(0xFFFFFFFFU), EtherAddress());
      else if (cp_ethernet_address(words[j], &ena, this)) {
	if (have_ena)
	  errh->error("argument %d has more than one Ethernet address", i);
	have_ena = true;
      } else {
	errh->error("argument %d should be `IP/MASK ETHADDR'", i);
	j = words.size();
      }

    // check for an argument that is both IP address and Ethernet address
    for (int j = 0; !have_ena && j < words.size(); j++)
      if (cp_ethernet_address(words[j], &ena, this))
	have_ena = true;
    
    if (first == _v.size())
      errh->error("argument %d had no IP address and masks", i);
    if (!have_ena)
      errh->error("argument %d had no Ethernet addresses", i);
    for (int j = first; j < _v.size(); j++)
      _v[j]._ena = ena;
  }

  return (before == errh->nerrors() ? 0 : -1);
}

Packet *
ARPResponder::make_response(u_char tha[6], /* him */
                            u_char tpa[4],
                            u_char sha[6], /* me */
                            u_char spa[4],
			    Packet *p /* only used for annotations */)
{
  WritablePacket *q = Packet::make(sizeof(click_ether) + sizeof(click_ether_arp));
  if (q == 0) {
    click_chatter("in arp responder: cannot make packet!");
    assert(0);
  }
  
  // in case of FromLinux, set the device annotation: want to make it seem
  // that ARP response came from the device that the query arrived on
  q->set_device_anno(p->device_anno());
  
  memset(q->data(), '\0', q->length());
  click_ether *e = (click_ether *) q->data();
  click_ether_arp *ea = (click_ether_arp *) (e + 1);
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
    if (lookup(ipa, ena)) {
      q = make_response(ea->arp_sha, ea->arp_spa, ena.data(), ea->arp_tpa, p);
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
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<ARPResponder::Entry>;
#endif
