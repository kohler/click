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
#include <clicknet/ether.h>
CLICK_DECLS

#ifndef srcr_assert
#define srcr_assert(e) ((e) ? (void) 0 : srcr_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


SRCR::SRCR()
  :  Element(2,2), 
     _datas(0), 
     _databytes(0),
     _link_table(0),
     _link_stat(0), 
     _rx_stats(0)

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
                    cpIP6Address, "IP address", &_ip,
                    cpEthernetAddress, "Ethernet address", &_eth,
                    cpKeywords,
		    "RX", cpElement, "RXStats element", &_rx_stats,
		    "ETT", cpElement, "ETT element", &_ett,
                    0);

  if (_link_table && _link_table->cast("LinkTable") == 0) 
    return errh->error("LT LinkTable element is not a LinkTable");
  if (_rx_stats && _rx_stats->cast("RXStats") == 0) 
    return errh->error("RX element is not a RXStats");
  if (_ett && _ett->cast("ETT") == 0) 
    return errh->error("ETT element is not a ETT");

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
int
SRCR::get_metric(IP6Address n)
{
  if (_ett) {
    return _ett->get_metric(n);
  }
  return 1;
}
void
SRCR::update_link(IP6Address from, IP6Address to, int metric) 
{
  if (_ett) {
    _ett->update_link(from, to, metric);
  }

}


Packet *
SRCR::encap(const u_char *payload, u_long payload_len, Vector<IP6Address> r)
{
  int hops = r.size();
  int len = sr_pkt::len_with_data(hops, payload_len);
  WritablePacket *p = Packet::make(len);
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);

  memcpy(pk->ether_shost, _eth.data(), 6);
  EtherAddress eth_dest;
  if (!r[1].ether_address(eth_dest)) {
    click_chatter("couldn't get mac address for %s!\n", 
		  r[1].s().cc());
  }
  memcpy(pk->ether_dhost, eth_dest.data(), 6);
  pk->ether_type = htons(_et);

  pk->_type = PT_DATA;
  pk->_dlen = htons(payload_len);

  pk->set_num_hops(r.size());
  pk->set_next(1);
  int i;
  for(i = 0; i < hops; i++) {
    pk->set_hop(i, r[i]);
  }
  memcpy(pk->data(), payload, payload_len);
  return p;
}

void
SRCR::push(int port, Packet *p_in)
{
  if (port > 1) {
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


  if (pk->next() >= pk->num_hops()){
    click_chatter("SRCR %s: data with bad next hop\n", 
		  _ip.s().cc());
    p_in->kill();
    return;
  }
  click_chatter("SRCR: %s checkpoint 1", 
		_ip.s().cc());

  if(port == 0 && pk->get_hop(pk->next()) != _ip){
    // it's not for me. these are supposed to be unicast,
    // so how did this get to me?
    click_chatter("SRCR %s: data not for me %d/%d %s",
		  _ip.s().cc(),
		  pk->next(),
		  pk->num_hops(),
		  pk->get_hop(pk->next()).s().cc());
    p_in->kill();
    return;
  }
  click_chatter("SRCR: %s: data passed basic checks", 
		_ip.s().cc());

  /* update the metrics from the packet */
  for(int i = 0; i < pk->num_hops()-1; i++) {
    IP6Address a = pk->get_hop(i);
    IP6Address b = pk->get_hop(i+1);
    uint8_t m = pk->get_fwd_metric(i);
    if (m != 0 && _link_table) {
      click_chatter("updating %s <%d> %s", a.s().cc(), m, b.s().cc());
      update_link(a,b,m);
    }
  }
  
  if (port == 1) {
    /* we're just sniffing this packet */
    p_in->kill();
    return;
  }
  IP6Address neighbor = IP6Address();
  neighbor = IP6Address(pk->get_hop(pk->next()-1));
  u_short m = get_metric(neighbor);
  if (_link_table) {
    click_chatter("updating %s <%d> %s", neighbor.s().cc(), m,  _ip.s().cc());
    update_link(neighbor, _ip, m);
  }

  if(pk->next() == pk->num_hops() - 1){
    //click_chatter("got data from %s for me\n", pk->get_hop(0).s().cc());
    // I'm the ultimate consumer of this data.
    /* need to decap */
    WritablePacket *p_out = Packet::make(pk->data_len());
    if (p_out == 0){
      return;
    }
    memcpy(p_out->data(), pk->data(), pk->data_len());
    output(1).push(p_out);
    return;
  } 

  click_chatter("forwarding packet from %d to %d\n", 
		pk->get_hop(0).s().cc(), pk->get_hop(pk->num_hops() - 1).s().cc());
  /* add the last hop's data onto the metric */
  u_short last_hop_metric = get_metric(IP6Address(pk->get_hop(pk->next() - 1)));

  int len = pk->hlen_with_data();
  WritablePacket *p = Packet::make(len);
  if(p == 0) {
    p_in->kill();
    return ;
  }
  struct sr_pkt *pk_out = (struct sr_pkt *) p->data();
  memcpy(pk_out, pk, len);

  
  pk_out->set_fwd_metric(pk->next() - 1, last_hop_metric);
  pk_out->set_next(pk->next() + 1);

  pk_out->ether_type = htons(_et);
  memcpy(pk_out->ether_shost, _eth.data(), 6);

  srcr_assert(pk->next() < 8);
  IP6Address nxt = pk->get_hop(pk->next());
  EtherAddress eth_dest;
  if (!nxt.ether_address(eth_dest)) {
    click_chatter("couldn't get mac address for %s!\n", 
		  nxt.s().cc());
  }
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
// generate Vector template instance
#include <click/vector.cc>
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<Path, SRCR::PathInfo>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SRCR)
