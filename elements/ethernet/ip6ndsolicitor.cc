/*
 * ip6ndsolicitor.{cc,hh} -- Neighborhood Solicitation element
 * Peilei Fan
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "ip6ndsolicitor.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ip6address.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

IP6NDSolicitor::IP6NDSolicitor()
: _expire_timer(expire_hook, this)
{
    // input 0: IP6 packets
    // input 1: ether/N.Advertisement responses
    // output 0: ether/IP6 and ether/N.Solicitation queries
  for (int i = 0; i < NMAP; i++)
    _map[i] = 0;
}

IP6NDSolicitor::~IP6NDSolicitor()
{
}

int
IP6NDSolicitor::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("IP", _my_ip6)
	.read_mp("ETH", _my_en)
	.complete();
}

int
IP6NDSolicitor::initialize(ErrorHandler *)
{
  _expire_timer.initialize(this);
  _expire_timer.schedule_after_msec(EXPIRE_TIMEOUT_MS);
  _arp_queries = 0;
  _pkts_killed = 0;
  return 0;
}

void
IP6NDSolicitor::cleanup(CleanupStage)
{
  for (int i = 0; i < NMAP; i++) {
    for (NDEntry *t = _map[i]; t; ) {
      NDEntry *n = t->next;
      if (t->p)
	t->p->kill();
      delete t;
      t = n;
    }
    _map[i] = 0;
  }
}

void
IP6NDSolicitor::take_state(Element *e, ErrorHandler *)
{
  IP6NDSolicitor *arpq = (IP6NDSolicitor *)e->cast("IP6NDSolicitor");
  if (!arpq || _my_ip6 != arpq->_my_ip6 || _my_en != arpq->_my_en)
    return;

  NDEntry *save[NMAP];
  memcpy(save, _map, sizeof(NDEntry *) * NMAP);
  memcpy(_map, arpq->_map, sizeof(NDEntry *) * NMAP);
  memcpy(arpq->_map, save, sizeof(NDEntry *) * NMAP);
}

void
IP6NDSolicitor::expire_hook(Timer *, void *thunk)
{
  IP6NDSolicitor *arpq = (IP6NDSolicitor *)thunk;
  click_jiffies_t jiff = click_jiffies();
  for (int i = 0; i < NMAP; i++) {
    NDEntry *prev = 0;
    while (1) {
      NDEntry *e = (prev ? prev->next : arpq->_map[i]);
      if (!e)
	break;
      if (e->ok) {
	int gap = jiff - e->last_response_jiffies;
	if (gap > 120*CLICK_HZ) {
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
  arpq->_expire_timer.schedule_after_msec(EXPIRE_TIMEOUT_MS);
}

void
IP6NDSolicitor::send_query_for(const u_char want_ip6[16])
{
  click_ether *e;
  click_ip6 *ip6;
  click_nd_sol *ea;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*ip6) + sizeof(*ea));
  if (q == 0) {
    click_chatter("in ndsol: cannot make packet!");
    assert(0);
  }

  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ip6=(click_ip6 *)(e+1);
  ea = (click_nd_sol *) (ip6 + 1);

  // set ethernet header
  // dst add is a multicast add: first two octets : 0x3333,
  // last four octets is the lst four octets of DST IP6Address
  // which is the solicited-node multicast address: "ff02::1:ff00:0" +
  // 24 bits from targest ip6 address
  e->ether_dhost[0] = 0x33;
  e->ether_dhost[1] = 0x33;
  e->ether_dhost[2] = 0xff;
  e->ether_dhost[3] = want_ip6[13];
  e->ether_dhost[4] = want_ip6[14];
  e->ether_dhost[5] = want_ip6[15];
  memcpy(e->ether_shost, _my_en.data(), 6);
  e->ether_type = htons(ETHERTYPE_IP6);

  // set ip6 header
  ip6->ip6_flow = 0;		// set flow to 0 (includes version)
  ip6->ip6_v = 6;		// then set version to 6
  ip6->ip6_plen=htons(sizeof(click_nd_sol));
  ip6->ip6_nxt=0x3a; //i.e. protocal: icmp6 message
  ip6->ip6_hlim=0xff; //indicate no router has processed it
  ip6->ip6_src = _my_ip6;
  unsigned char  dst2[16];
  dst2[0]=0xff;
  dst2[1]=0x02;
  for (int i=2; i<11; i++) {
    dst2[i]=0;
  }
  dst2[11]=1;
  dst2[12]=0xff;
  dst2[13]=want_ip6[13];
  dst2[14]=want_ip6[14];
  dst2[15]=want_ip6[15];
  ip6->ip6_dst = IP6Address(dst2);

  //set ICMP6 - Neighborhood Solicitation Message
  ea->type = 0x87;
  ea->code =0;
  ea->reserved = htonl(0);
  memcpy(ea->nd_tpa, want_ip6, 16);
  ea->option_type = 0x1;
  ea->option_length = 0x1;
  memcpy(ea->nd_sha, _my_en.data(), 6);

  ea->checksum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, 0, (unsigned char *)(ip6+1), htons(sizeof(click_nd_sol))));

  _arp_queries++;
  output(noutputs()-1).push(q);
}

/*
 * If the packet's IP6 address is in the table, add an ethernet header
 * and push it out.
 * Otherwise push out a query packet.
 * May save the packet in the NDEntry table for later sending.
 * May call p->kill().
 */
void
IP6NDSolicitor::handle_ip6(Packet *p)
{
  IP6Address ipa = DST_IP6_ANNO(p);
  int bucket = (ipa.data()[0] + ipa.data()[15]) % NMAP;
  NDEntry *ae = _map[bucket];
  while (ae && ae->ip6 != ipa)
    ae = ae->next;

  if (ae) {
    if (ae->polling) {
      send_query_for(ae->ip6.data());
      ae->polling = 0;
    }
    //find the match IP address, send to output 0
    if (ae->ok) {
      Packet *q = p->push(sizeof(click_ether));
      click_ether *e = (click_ether *)q->data();
      memcpy(e->ether_shost, _my_en.data(), 6);
      memcpy(e->ether_dhost, ae->en.data(), 6);
      e->ether_type = htons(ETHERTYPE_IP6);
      output(0).push(q);
    } else {
      if (ae->p) {
        ae->p->kill();
	_pkts_killed++;
      }
      ae->p = p;
      send_query_for(DST_IP6_ANNO(p).data());
    }

  } else {
    NDEntry *ae = new NDEntry;
    ae->ip6 = ipa;
    ae->ok = ae->polling = 0;
    ae->p = p;
    ae->next = _map[bucket];
    _map[bucket] = ae;

    send_query_for(DST_IP6_ANNO(p).data());
  }
}

/*
 * Got an Neighborhood Advertisement (response to N. Solicitation Message)
 * Update our NDEntry table.
 * If there was a packet waiting to be sent, return it.
 */
void
IP6NDSolicitor::handle_response(Packet *p)
{
  if (p->length() < sizeof(click_ether) + sizeof(click_ip6) + sizeof(click_nd_sol))
    return;

  click_ether *ethh = (click_ether *) p->data();
  click_ip6 *ip6h = (click_ip6 *)(ethh+1);
  click_nd_adv * eah = (click_nd_adv*)(ip6h+1);

  IP6Address ipa = IP6Address(eah->nd_tpa);
  EtherAddress ena = EtherAddress(eah->nd_tha);
    if (ntohs(ethh->ether_type) == ETHERTYPE_IP6
	&& eah->type == ND_ADV) {
//        && !ena.is_group()) {
     int bucket = (ipa.data()[0] + ipa.data()[15]) % NMAP;
      NDEntry *ae = _map[bucket];
      while (ae && ae->ip6 != ipa)
        ae = ae->next;
      if (!ae)
        return;

      if (ae->ok && ae->en != ena)
        click_chatter("IP6NDSolicitor overwriting an entry");
      ae->en = ena;
      ae->ok = 1;
      ae->polling = 0;
      ae->last_response_jiffies = click_jiffies();
      Packet *cached_packet = ae->p;
      ae->p = 0;

      if (cached_packet){
        handle_ip6(cached_packet);}
    }
}

void
IP6NDSolicitor::push(int port, Packet *p)
{
   if (port == 0){
     handle_ip6(p); }
  else {
    handle_response(p);
    p->kill();
  }
}

String
IP6NDSolicitor::read_table(Element *e, void *)
{
    IP6NDSolicitor *q = (IP6NDSolicitor *)e;
    StringAccum sa;
    for (int i = 0; i < NMAP; i++)
	for (NDEntry *e = q->_map[i]; e; e = e->next)
	    sa << e->ip6 << ' ' << (e->ok ? 1 : 0) << ' ' << e->en << '\n';
    return sa.take_string();
}

static String
IP6NDSolicitor_read_stats(Element *e, void *)
{
  IP6NDSolicitor *q = (IP6NDSolicitor *)e;
  return
    String(q->_pkts_killed) + " packets killed\n" +
    String(q->_arp_queries) + " ND Solicitation Message sent\n";
}

void
IP6NDSolicitor::add_handlers()
{
  add_read_handler("table", read_table, 0);
  add_read_handler("stats", IP6NDSolicitor_read_stats, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ip6)
EXPORT_ELEMENT(IP6NDSolicitor)
