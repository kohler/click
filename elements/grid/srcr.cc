/*
 * SRCR.{cc,hh} -- DSR implementation
 * Robert Morris
 * John Bicket
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
#include "srcr.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/linkstat.hh>
#include <click/straccum.hh>
#include <elements/grid/arptable.hh>
#include <clicknet/ether.h>
CLICK_DECLS

#ifndef srcr_assert
#define srcr_assert(e) ((e) ? (void) 0 : srcr_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


SRCR::SRCR()
  :  Element(1,2), _datas(0), _databytes(0),
     _link_stat(0), _arp_table(0)
{
  MOD_INC_USE_COUNT;
}

SRCR::~SRCR()
{
  MOD_DEC_USE_COUNT;
}

int
SRCR::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int res;
  res = cp_va_parse(conf, this, errh,
		    cpUnsigned, "Ethernet encapsulation type", &_et,
                    cpIPAddress, "IP address", &_ip,
                    cpEthernetAddress, "Ethernet address", &_eth,
		    cpElement, "LinkTable element", &_link_table,
		    cpElement, "ARPTable element", &_arp_table,
                    cpKeywords,
		    "LS", cpElement, "LinkStat element", &_link_stat,
                    0);

  if (res < 0) {
    return res;
  }
  return res;
}

SRCR *
SRCR::clone () const
{
  return new SRCR;
}

int
SRCR::initialize (ErrorHandler *)
{
  return 0;
}

// Ask LinkStat for the metric for the link from other to us.
u_short
SRCR::get_metric(IPAddress other)
{
  u_short dft = 9999; // default metric
  if(_link_stat){
    unsigned int tau;
    struct timeval tv;
    unsigned int frate, rrate;
    bool res = _link_stat->get_forward_rate(other, &frate, &tau, &tv);
    if(res == false) {
      return dft;
    }
    res = _link_stat->get_reverse_rate(other, &rrate, &tau);
    if(res == false) {
      return dft;
    }
    if(frate == 0 || rrate == 0) {
      return dft;
    }
    u_short m = 100 * 100 * 100 / (frate * (int) rrate);
    return m;
  } else {
    click_chatter("no link stat!!!!");
    return dft;
  }
}


Packet *
SRCR::encap(const u_char *payload, u_long payload_len, Vector<IPAddress> r)
{
  int hops = r.size();
  int len = sr_pkt::len_with_data(hops, payload_len);
  WritablePacket *p = Packet::make(len);
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);
  pk->_type = PT_DATA;
  pk->_dlen = htons(payload_len);
  pk->_nhops = htons(hops);
  pk->_next = htons(1);
  int i;
  for(i = 0; i < hops; i++) {
    pk->set_hop(i, r[i].in_addr());
  }
  memcpy(pk->data(), payload, payload_len);
  return p;
}

void
SRCR::push(int port, Packet *p_in)
{
  if (port != 0) {
    p_in->kill();
    return;
  }
  struct sr_pkt *pk = (struct sr_pkt *) p_in->data();
  //click_chatter("SRCR %s: got sr packet", _ip.s().cc());
  if(p_in->length() < 20 || p_in->length() < pk->hlen_wo_data()){
    click_chatter("SRCR %s: bad sr_pkt len %d, expected %d",
                  _ip.s().cc(),
                  p_in->length(),
		  pk->hlen_wo_data());
    p_in->kill();
    return;
  }
  if(pk->ether_type != htons(_et)){
    click_chatter("SRCR %s: bad ether_type %04x",
                  _ip.s().cc(),
                  ntohs(pk->ether_type));
    p_in->kill();
    return;
  }

  if (pk->_type != PT_DATA) {
    click_chatter("SRCR %s: bad packet_type %04x",
                  _ip.s().cc(),
                  pk->_type);
    p_in->kill();
    return ;
  }

  u_short nhops = ntohs(pk->_nhops);
  u_short next = ntohs(pk->_next);

  if (next >= nhops){
    p_in->kill();
    return;
  }

  if(pk->next() + 1 >= pk->num_hops()) {
    click_chatter("SRCR %s: forward_data strange next=%d, nhops=%d", 
		  _ip.s().cc(), 
		  pk->next() + 1,
		  pk->num_hops());
    p_in->kill();
    return ;
  }



  if(pk->get_hop(next) != _ip.in_addr()){
    // it's not for me. these are supposed to be unicast,
    // so how did this get to me?
    click_chatter("SRCR %s: data not for me %d/%d %s",
		  _ip.s().cc(),
		  ntohs(pk->_next),
		  ntohs(pk->_nhops),
		  IPAddress(pk->get_hop(next)).s().cc());
    p_in->kill();
    return;
  }
  

  /* update the metrics from the packet */
  unsigned int now = click_jiffies();
  for(int i = 0; i < pk->num_hops()-1; i++) {
    IPAddress a = pk->get_hop(i);
    IPAddress b = pk->get_hop(i+1);
    u_short m = pk->get_metric(i);
    if (m != 0) {
      //click_chatter("updating %s <%d> %s", a.s().cc(), m, b.s().cc());
      update_link(IPPair(a,b), m, now);
    }
  }
  
  IPAddress neighbor = IPAddress(0);
  neighbor = IPAddress(pk->get_hop(pk->next()-1));
  u_short m = get_metric(neighbor);
  //click_chatter("updating %s <%d> %s", neighbor.s().cc(), m,  _ip.s().cc());
  update_link(IPPair(neighbor, _ip), m, now);

  _arp_table->insert(neighbor, EtherAddress(pk->ether_shost));

  if(next == nhops -1){
    // I'm the ultimate consumer of this data.
    output(1).push(p_in);
    return;
  } 

  
  /* add the last hop's data onto the metric */
  u_short last_hop_metric = get_metric(IPAddress(pk->get_hop(pk->next() - 1)));

  int len = pk->hlen_with_data();
  WritablePacket *p = Packet::make(len);
  if(p == 0) {
    p_in->kill();
    return ;
  }
  struct sr_pkt *pk_out = (struct sr_pkt *) p->data();
  memcpy(pk_out, pk, len);

  
  pk_out->set_metric(pk->next() - 1, last_hop_metric);
  pk_out->set_next(pk->next() + 1);

  pk_out->ether_type = htons(_et);
  memcpy(pk_out->ether_shost, _eth.data(), 6);

  srcr_assert(next < 8);
  struct in_addr nxt = pk->get_hop(next);
  EtherAddress eth_dest = _arp_table->lookup(IPAddress(nxt));
  memcpy(pk_out->ether_dhost, eth_dest.data(), 6);

  p_in->kill();
  output(0).push(p);
  return;


}

String
SRCR::static_print_stats(Element *f, void *)
{
  SRCR *d = (SRCR *) f;
  return d->print_stats();
}

String
SRCR::print_stats()
{
  
  return
    String(_datas) + " datas sent\n" +
    String(_databytes) + " bytes of data sent\n";

}

void
SRCR::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
}
void
SRCR::update_link(IPPair p, u_short m, unsigned int now)
{
  _link_table->update_link(p, m, now);
}

void
SRCR::srcr_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("SRCR %s assertion \"%s\" failed: file %s, line %d",
		id().cc(), expr, file, line);
#ifdef CLICK_USERLEVEL  
  abort();
#else
  click_chatter("Continuing execution anyway, hold on to your hats!\n");
#endif

}

CLICK_ENDDECLS
EXPORT_ELEMENT(SRCR)
