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
  :  Element(1,2), 
     _datas(0), 
     _databytes(0),
     _arp_table(0),
     _ett(0)

{
  MOD_INC_USE_COUNT;

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
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
		    cpEtherAddress, "Ethernet Address", &_eth,
		    cpElement, "ARPTable element", &_arp_table,
                    cpKeywords,
		    "ETT", cpElement, "ETT element", &_ett,
                    0);

  if (_arp_table && _arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not a Arptable");
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
SRCR::get_metric(IPAddress other)
{
  if (_ett) {
    return _ett->get_metric(other);
  }
  return 0;
}

void
SRCR::update_link(IPAddress from, IPAddress to, int metric) 
{
  if (_ett) {
    _ett->update_link(from, to, metric);
    _ett->update_link(to, from, metric);
  }

}


Packet *
SRCR::encap(const u_char *payload, u_long payload_len, Vector<IPAddress> r)
{
  int hops = r.size();
  int len = sr_pkt::len_with_data(hops, payload_len);
  srcr_assert(r.size() > 1);
  WritablePacket *p = Packet::make(len);
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);

  memcpy(pk->ether_shost, _eth.data(), 6);
  EtherAddress eth_dest = _arp_table->lookup(r[1]);
  memcpy(pk->ether_dhost, eth_dest.data(), 6);
  pk->ether_type = htons(_et);
  pk->_version = _srcr_version;
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



  if(port == 0 && pk->get_hop(pk->next()) != _ip){
    if (pk->get_dhost() != _bcast) {
      /* 
       * if the arp doesn't have a ethernet address, it
       * will broadcast the packet. in this case,
       * don't complain. But otherwise, something's up
       */
      click_chatter("SRCR %s: data not for me %d/%d ip %s eth %s",
		    _ip.s().cc(),
		    pk->next(),
		    pk->num_hops(),
		    pk->get_hop(pk->next()).s().cc(),
		    pk->get_dhost().s().cc());
    }
    p_in->kill();
    return;
  }

  /* update the metrics from the packet */
  for(int i = 0; i < pk->num_hops()-1; i++) {
    IPAddress a = pk->get_hop(i);
    IPAddress b = pk->get_hop(i+1);
    int m = pk->get_metric(i);
    if (m != 0 && a != _ip && b != _ip) {
      /* 
       * don't update the link for my neighbor
       * we'll do that below
       */
      //click_chatter("updating %s -> %d -> %s", a.s().cc(), m, b.s().cc());
      update_link(a,b,m);
    }
  }
  

  IPAddress prev = pk->get_hop(pk->next()-1);
  _arp_table->insert(prev, pk->get_shost());

  int prev_metric = get_metric(prev);
  update_link(_ip, prev, prev_metric);

  if(pk->next() == pk->num_hops() - 1){
    //click_chatter("got data from %s for me\n", pk->get_hop(0).s().cc());
    // I'm the ultimate consumer of this data.
    /* need to decap */
    WritablePacket *p_out = Packet::make(pk->data_len());
    if (p_out == 0){
      click_chatter("SRCR %s: couldn't make packet\n", id().cc());
      p_in->kill();
      return;
    }
    memcpy(p_out->data(), pk->data(), pk->data_len());
    output(1).push(p_out);
    p_in->kill();
    return;
  } 

  int len = pk->hlen_with_data();
  WritablePacket *p = Packet::make(len);
  if(p == 0) {
    click_chatter("SRCR %s: couldn't make packet\n", id().cc());
    p_in->kill();
    return ;
  }
  struct sr_pkt *pk_out = (struct sr_pkt *) p->data();
  memcpy(pk_out, pk, len);

  pk_out->set_metric(pk_out->next() - 1, prev_metric);
  pk_out->set_next(pk->next() + 1);
  pk_out->ether_type = htons(_et);

  srcr_assert(pk->next() < 8);
  IPAddress nxt = pk_out->get_hop(pk_out->next());



  EtherAddress eth_dest = _arp_table->lookup(nxt);
  memcpy(pk_out->ether_dhost, eth_dest.data(), 6);
  memcpy(pk_out->ether_shost, _eth.data(), 6);

  /* set the ip header anno */
  const click_ip *ip = reinterpret_cast<const click_ip *>
    (pk_out->data());
  p->set_ip_header(ip, pk_out->data_len());


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
