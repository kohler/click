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
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
CLICK_DECLS

#ifndef srcr_assert
#define srcr_assert(e) ((e) ? (void) 0 : srcr_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


SRCR::SRCR()
  :  Element(1,2), 
     _datas(0), 
     _databytes(0),
     _link_table(0),
     _arp_table(0),
     _ett(0),
     _srcr_stat(0)

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
		    "SS", cpElement, "SrcrStat element", &_srcr_stat,
		    "ETT", cpElement, "ETT element", &_ett,
		    "LT", cpElement, "LinkTable element", &_link_table,
                    0);

  if (_arp_table && _arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not a Arptable");
  if (_srcr_stat && _srcr_stat->cast("SrcrStat") == 0) 
    return errh->error("SS element is not a SrcrStat");
  if (_ett && _ett->cast("ETT") == 0) 
    return errh->error("ETT element is not a Grid");
  if (_link_table && _link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");

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
  int metric = 9999;
  srcr_assert(other);
  if (_srcr_stat) {
    metric = _srcr_stat->get_etx(other);
  }
  update_link(_ip, other, metric);
  return metric;
}

void
SRCR::update_link(IPAddress from, IPAddress to, int metric) 
{
  if (_link_table) {
    _link_table->update_link(from, to, metric);
    _link_table->update_link(to, from, metric);
  }

}


Packet *
SRCR::encap(const u_char *payload, u_long payload_len, Vector<IPAddress> r)
{
  int hops = r.size();
  int len = sr_pkt::len_with_data(hops, payload_len);
  srcr_assert(r.size() > 1);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
  memset(pk, '\0', len);

  memcpy(eh->ether_shost, _eth.data(), 6);
  EtherAddress eth_dest = _arp_table->lookup(r[1]);
  memcpy(eh->ether_dhost, eth_dest.data(), 6);
  eh->ether_type = htons(_et);
  pk->_version = _srcr_version;
  pk->_type = PT_DATA;
  pk->_dlen = htons(payload_len);

  if (_ett) {
    IPAddress neighbor = _ett->get_random_neighbor();
    if (neighbor) {
      pk->set_random_from(_ip);
      pk->set_random_to(neighbor);
      pk->set_random_metric(get_metric(neighbor));
    }
  }
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
  click_ether *eh = (click_ether *) p_in->data();
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);

  if(eh->ether_type != htons(_et)){
    click_chatter("SRCR %s: bad ether_type %04x",
                  _ip.s().cc(),
                  ntohs(eh->ether_type));
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
    if (EtherAddress(eh->ether_dhost) != _bcast) {
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
		    EtherAddress(eh->ether_dhost).s().cc());
    }
    p_in->kill();
    return;
  }

  /* update the metrics from the packet */
  IPAddress r_from = pk->get_random_from();
  IPAddress r_to = pk->get_random_to();
  int r_metric = pk->get_random_metric();
  if (r_from && r_to) {
    update_link(r_from, r_to, r_metric);
  }

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
  _arp_table->insert(prev, EtherAddress(eh->ether_shost));

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

    /* set the dst to the gateway it came from 
     * this is kinda weird.
     */
    SET_MISC_IP_ANNO(p_out, pk->get_hop(0));

    output(1).push(p_out);
    p_in->kill();
    return;
  } 

  int len = pk->hlen_with_data();
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0) {
    click_chatter("SRCR %s: couldn't make packet\n", id().cc());
    p_in->kill();
    return ;
  }
  click_ether *eh_out = (click_ether *) p->data();
  struct sr_pkt *pk_out = (struct sr_pkt *) (eh_out+1);
  memcpy(pk_out, pk, len);

  pk_out->set_metric(pk_out->next() - 1, prev_metric);
  pk_out->set_next(pk->next() + 1);
  pk_out->set_num_hops(pk->num_hops());
  eh_out->ether_type = htons(_et);

  srcr_assert(pk->next() < 8);
  IPAddress nxt = pk_out->get_hop(pk_out->next());
  
  /*
   * put new information in the random link field
   * with probability = 1/num_hops in the packet
   */
  if (_ett && random() % pk_out->num_hops() == 0) {
    IPAddress r_neighbor = _ett->get_random_neighbor();
    if (r_neighbor) {
      pk_out->set_random_from(_ip);
      pk_out->set_random_to(r_neighbor);
      pk_out->set_random_metric(get_metric(r_neighbor));
    }
  }


  EtherAddress eth_dest = _arp_table->lookup(nxt);
  memcpy(eh_out->ether_dhost, eth_dest.data(), 6);
  memcpy(eh_out->ether_shost, _eth.data(), 6);

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

#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SRCR)
