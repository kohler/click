/*
 * arpquerier.{cc,hh} -- ARP resolver element
 * Robert Morris, Eddie Kohler
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
#include "arpquerier.hh"
#include "click_ether.h"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "bitvector.hh"
#include "error.hh"
#include "glue.hh"

ARPQuerier::ARPQuerier()
  : _expire_timer(expire_hook, (unsigned long)this)
{
  add_input(); /* IP packets */
  add_input(); /* ether/ARP responses */
  add_output();/* ether/IP and ether/ARP queries */
  for (int i = 0; i < NMAP; i++)
    _map[i] = 0;
}

ARPQuerier::~ARPQuerier()
{
  uninitialize();
}

Bitvector
ARPQuerier::forward_flow(int i) const
{
  Bitvector bv(noutputs(), false);
  // Packets can flow from input 0 to output 0
  if (i == 0) bv[0] = true;
  return bv;
}

Bitvector
ARPQuerier::backward_flow(int o) const
{
  Bitvector bv(2, false);
  if (o == 0) bv[0] = true;
  return bv;
}

ARPQuerier *
ARPQuerier::clone() const
{
  return new ARPQuerier;
}

void
ARPQuerier::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
ARPQuerier::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpIPAddress, "IP address", &_my_ip,
		     cpEthernetAddress, "Ethernet address", &_my_en,
		     0);
}

int
ARPQuerier::initialize(ErrorHandler *)
{
  _expire_timer.attach(this);
  _expire_timer.schedule_after_ms(EXPIRE_TIMEOUT_MS);
  _arp_queries = 0;
  _pkts_killed = 0;
  return 0;
}

void
ARPQuerier::uninitialize()
{
  _expire_timer.unschedule();
  for (int i = 0; i < NMAP; i++) {
    for (ARPEntry *t = _map[i]; t; ) {
      ARPEntry *n = t->next;
      if (t->p)
	t->p->kill();
      delete t;
      t = n;
    }
    _map[i] = 0;
  }
}

void
ARPQuerier::take_state(Element *e, ErrorHandler *)
{
  ARPQuerier *arpq = (ARPQuerier *)e->cast("ARPQuerier");
  if (!arpq || _my_ip != arpq->_my_ip || _my_en != arpq->_my_en)
    return;

  ARPEntry *save[NMAP];
  memcpy(save, _map, sizeof(ARPEntry *) * NMAP);
  memcpy(_map, arpq->_map, sizeof(ARPEntry *) * NMAP);
  memcpy(arpq->_map, save, sizeof(ARPEntry *) * NMAP);
}

void
ARPQuerier::expire_hook(unsigned long thunk)
{
  ARPQuerier *arpq = (ARPQuerier *)thunk;
  int jiff = click_jiffies();
  for (int i = 0; i < NMAP; i++) {
    ARPEntry *prev = 0;
    while (1) {
      ARPEntry *e = (prev ? prev->next : arpq->_map[i]);
      if (!e)
	break;
      if (e->ok) {
	int gap = jiff - e->last_response_jiffies;
	if (gap > 120*CLICK_HZ) {
	  click_chatter("ARPQuerier timing out %x", e->ip.addr());
	  // delete entry from map
	  if (prev) prev->next = e->next;
	  else arpq->_map[i] = e->next;
	  if (e->p)
	    e->p->kill();
	  delete e;
	  continue;		// don't change prev
	} else if (gap > 60*CLICK_HZ)
	  e->polling = 1;
      }
      prev = e;
    }
  }
  arpq->_expire_timer.schedule_after_ms(EXPIRE_TIMEOUT_MS);
}

void
ARPQuerier::send_query_for(const IPAddress &want_ip)
{
  click_ether *e;
  click_ether_arp *ea;
  Packet *q = Packet::make(sizeof(*e) + sizeof(*ea));
  if (q == 0) {
    click_chatter("in arp querier: cannot make packet!");
    assert(0);
  } 
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ea = (click_ether_arp *) (e + 1);
  memcpy(e->ether_dhost, "\xff\xff\xff\xff\xff\xff", 6);
  memcpy(e->ether_shost, _my_en.data(), 6);
  e->ether_type = htons(ETHERTYPE_ARP);
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
  ea->ea_hdr.ar_hln = 6;
  ea->ea_hdr.ar_pln = 4;
  ea->ea_hdr.ar_op = htons(ARPOP_REQUEST);
  memcpy(ea->arp_tpa, want_ip.data(), 4);
  memcpy(ea->arp_sha, _my_en.data(), 6);
  memcpy(ea->arp_spa, _my_ip.data(), 4);
  _arp_queries++;
  output(noutputs()-1).push(q);
}

/*
 * If the packet's IP address is in the table, add an ethernet header
 * and push it out.
 * Otherwise push out a query packet.
 * May save the packet in the ARP table for later sending.
 * May call p->kill().
 */
void
ARPQuerier::handle_ip(Packet *p)
{
  IPAddress ipa = p->dst_ip_anno();
  int bucket = (ipa.data()[0] + ipa.data()[3]) % NMAP;
  ARPEntry *ae = _map[bucket];
  while (ae && ae->ip != ipa)
    ae = ae->next;

  if (ae) {
    if (ae->polling) {
      send_query_for(ae->ip);
      ae->polling = 0;
    }
    
    if (ae->ok) {
      Packet *q = p->push(sizeof(click_ether));
      click_ether *e = (click_ether *)q->data();
      memcpy(e->ether_shost, _my_en.data(), 6);
      memcpy(e->ether_dhost, ae->en.data(), 6);
      e->ether_type = htons(ETHERTYPE_IP);
      output(0).push(q);
    } else {
      if (ae->p) {
        ae->p->kill();
	_pkts_killed++;
      }
      ae->p = p;
      send_query_for(p->dst_ip_anno());
    }
    
  } else {
    ARPEntry *ae = new ARPEntry;
    ae->ip = ipa;
    ae->ok = ae->polling = 0;
    ae->p = p;
    ae->next = _map[bucket];
    _map[bucket] = ae;
    send_query_for(p->dst_ip_anno());
  }
}

/*
 * Got an ARP response.
 * Update our ARP table.
 * If there was a packet waiting to be sent, return it.
 */
void
ARPQuerier::handle_response(Packet *p)
{
  if (p->length() < sizeof(click_ether) + sizeof(click_ether_arp))
    return;
  
  click_ether *ethh = (click_ether *) p->data();
  click_ether_arp *arph = (click_ether_arp *) (ethh + 1);
  IPAddress ipa = IPAddress(arph->arp_spa);
  EtherAddress ena = EtherAddress(arph->arp_sha);
  if (ntohs(ethh->ether_type) == ETHERTYPE_ARP
      && ntohs(arph->ea_hdr.ar_hrd) == ARPHRD_ETHER
      && ntohs(arph->ea_hdr.ar_pro) == ETHERTYPE_IP
      && ntohs(arph->ea_hdr.ar_op) == ARPOP_REPLY
      && !ena.is_group()) {
    
    int bucket = (ipa.data()[0] + ipa.data()[3]) % NMAP;
    ARPEntry *ae = _map[bucket];
    while (ae && ae->ip != ipa)
      ae = ae->next;
    if (!ae)
      return;
    
    if (ae->ok && ae->en != ena)
      click_chatter("ARPQuerier overwriting an entry");
    ae->en = ena;
    ae->ok = 1;
    ae->polling = 0;
    ae->last_response_jiffies = click_jiffies();
    Packet *cached_packet = ae->p;
    ae->p = 0;
    if (cached_packet)
      handle_ip(cached_packet);
  }
}

void
ARPQuerier::push(int port, Packet *p)
{
  if (port == 0)
    handle_ip(p);
  else {
    handle_response(p);
    p->kill();
  }
}

String
ARPQuerier::read_table(Element *e, void *)
{
  ARPQuerier *q = (ARPQuerier *)e;
  String s;
  for (int i = 0; i < NMAP; i++)
    for (ARPEntry *e = q->_map[i]; e; e = e->next) {
      s += e->ip.s() + " " + (e->ok ? "1" : "0") + " " + e->en.s() + "\n";
    }
  return s;
}

static String
ARPQuerier_read_stats(Element *e, void *)
{
  ARPQuerier *q = (ARPQuerier *)e;
  return
    String(q->_pkts_killed) + " packets killed\n" +
    String(q->_arp_queries) + " ARP queries sent\n";
}

void
ARPQuerier::add_handlers()
{
  add_read_handler("table", read_table, (void *)0);
  add_read_handler("stats", ARPQuerier_read_stats, (void *)0);
}

EXPORT_ELEMENT(ARPQuerier)
