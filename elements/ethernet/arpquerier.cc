/*
 * arpquerier.{cc,hh} -- ARP resolver element
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
#include "arpquerier.hh"
#include "click_ether.h"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "bitvector.hh"
#include "error.hh"
#include "glue.hh"

ARPQuerier::ARPQuerier()
{
  add_input(); /* IP packets */
  add_input(); /* ether/ARP responses */
  add_output();/* ether/IP and ether/ARP queries */
  _arp_queries = 0;
  _pkts_killed = 0;
}

ARPQuerier::~ARPQuerier()
{
}

Bitvector
ARPQuerier::forward_flow(int i) const
{
  // Packets can flow from input 0 to output 0
  return Bitvector(1, i == 0);
}

Bitvector
ARPQuerier::backward_flow(int o) const
{
  Bitvector bv(2, false);
  if (o == 0) bv[0] = true;
  return bv;
}

int
ARPQuerier::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  
  if (args.size() != 2) {
    errh->error("ARPQuerier conf needs IP addr and ethernet addr");
    return(-1);
  }
  
  if (cp_ip_address(args[0], _my_ip)
      && cp_ethernet_address(args[1], _my_en)) {
    return(0);
  } else {
    errh->error("ARPQuerier conf needs IP addr and ethernet addr");
    return(-1);
  }
}

ARPQuerier *
ARPQuerier::clone() const
{
  return new ARPQuerier;
}

Packet *
ARPQuerier::make_query(u_char tpa[4], /* him */
                       u_char sha[6], /* me */
                       u_char spa[4])
{
  struct ether_header *e;
  struct ether_arp *ea;
  Packet *q = Packet::make(sizeof(*e) + sizeof(*ea));
  if (q == 0) {
    click_chatter("in arp querier: cannot make packet!");
    assert(0);
  } 
  memset(q->data(), '\0', q->length());
  e = (struct ether_header *) q->data();
  ea = (struct ether_arp *) (e + 1);
  memcpy(e->ether_dhost, "\xff\xff\xff\xff\xff\xff", 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_ARP);
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
  ea->ea_hdr.ar_hln = 6;
  ea->ea_hdr.ar_pln = 4;
  ea->ea_hdr.ar_op = htons(ARPOP_REQUEST);
  memcpy(ea->arp_tpa, tpa, 4);
  memcpy(ea->arp_sha, sha, 6);
  memcpy(ea->arp_spa, spa, 4);
  return(q);
}

/*
 * If the packet's IP address is in the table, return a new
 * packet with an ethernet header.
 * Otherwise returns a query packet.
 * May save the packet in the ARP table for later sending.
 * May call p->kill().
 */
Packet *
ARPQuerier::lookup(Packet *p)
{
  IPAddress ipa = p->dst_ip_anno();
  ARPEntry **aep = _map.findp(ipa);
  if (aep) {
    ARPEntry *ae = *aep;

    // Time out old ARP entries.
    struct timeval now;
#ifdef __KERNEL__
    get_fast_time(&now);
#else
    click_gettimeofday(&now);
#endif
    if(ae->ok && now.tv_sec - ae->when.tv_sec > 120){
      click_chatter("ARPQuerier timing out %x", ipa.saddr());
      ae->ok = 0;
    }
    if(ae->ok && ae->polling == 0 && now.tv_sec - ae->when.tv_sec > 60){
      ae->polling = 1;
      _arp_queries++;
      output(0).push(query_for(p));
    }      

    if(ae->ok) {
      Packet *q = p->push(sizeof(struct ether_header));
      struct ether_header *e = (struct ether_header *)q->data();
      memcpy(e->ether_shost, _my_en.data(), 6);
      memcpy(e->ether_dhost, ae->a.data(), 6);
      e->ether_type = htons(ETHERTYPE_IP);
      return(q);
    } else {
      if(ae->p) {
        ae->p->kill();
	_pkts_killed++;
      }
      ae->p = p;
      _arp_queries++;
      return(query_for(p));
    }
  } else {
    ARPEntry *ae = new ARPEntry();
    ae->ok = 0;
    ae->p = p;
    _map.insert(ipa, ae);
    // click_chatter("%s sending arp query",declaration().cc());
    _arp_queries++;
    return(query_for(p));
  }
}

void
ARPQuerier::insert(IPAddress ipa, EtherAddress ena)
{
  ARPEntry *ae = new ARPEntry();
  ae->a = ena;
  ae->ok = 1;
  ae->p = 0;
  ae->polling = 0;
  _map.insert(ipa, ae);
}

Packet *
ARPQuerier::query_for(Packet *p)
{
  IPAddress ipa = p->dst_ip_anno();
  Packet *q = make_query(ipa.data(), _my_en.data(), _my_ip.data());
  return(q);
}

/*
 * Got an ARP response.
 * Update our ARP table.
 * If there was a packet waiting to be sent, return it.
 */
Packet *
ARPQuerier::response(Packet *p)
{
  if(p->length() < sizeof(struct ether_header) + sizeof(struct ether_arp))
    return(0);
  
  struct ether_header *e = (struct ether_header *) p->data();
  struct ether_arp *ea = (struct ether_arp *) (e + 1);
  IPAddress ipa = IPAddress(ea->arp_spa);
  EtherAddress ena = EtherAddress(ea->arp_sha);
  if (p->length() >= sizeof(*e) + sizeof(struct ether_arp) &&
      ntohs(e->ether_type) == ETHERTYPE_ARP &&
      ntohs(ea->ea_hdr.ar_hrd) == ARPHRD_ETHER &&
      ntohs(ea->ea_hdr.ar_pro) == ETHERTYPE_IP &&
      ntohs(ea->ea_hdr.ar_op) == ARPOP_REPLY &&
      ena.is_group() == 0 &&
      _map.findp(ipa)){
    // click_chatter("got an arp response for %s",ipa.s().cc());
    ARPEntry *ae = _map[ipa];
    if(ae->ok && ae->a != ena){
      click_chatter("ARPQuerier overwriting an entry");
    }
    ae->a = ena;
    ae->ok = 1;
    click_gettimeofday(&(ae->when));
    ae->polling = 0;
    Packet *q = ae->p;
    ae->p = 0;
    if(q){
      return(lookup(q));
    } else {
      return(0);
    }
  }
  
  return(0);
}

void
ARPQuerier::push(int port, Packet *p)
{
  if (port == 0){
    output(0).push(lookup(p));
  } else {
    Packet *q = response(p);
    p->kill();
    if (q) output(0).push(q);
  }
}

bool
ARPQuerier::each(int &i, IPAddress &k, ARPEntry &v) const
{
  ARPEntry *ae = 0;
  if(_map.each(i, k, ae)){
    v = *ae;
    return(true);
  }
  return(false);
}

static String
ARPQuerier_read_table(Element *f, void *)
{
  ARPQuerier *q = (ARPQuerier *)f;
  String s;
  
  int i = 0;
  IPAddress k;
  ARPQuerier::ARPEntry v;
  while(q->each(i, k, v)){
    s += k.s() + " " + (v.ok?"1":"0") + " " + v.a.s() + "\n";
  }

  return(s);
}

static String
ARPQuerier_read_pkts(Element *f, void *)
{
  ARPQuerier *q = (ARPQuerier *)f;
  return
    String(q->_pkts_killed) + " packets killed\n" +
    String(q->_arp_queries) + " arp queries sent\n";
}

void
ARPQuerier::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("table", ARPQuerier_read_table, (void *)0);
  fcr->add_read("pkts", ARPQuerier_read_pkts, (void *)0);
}


EXPORT_ELEMENT(ARPQuerier)

#include "hashmap.cc"
template class HashMap<IPAddress, ARPQuerier::ARPEntry *>;
