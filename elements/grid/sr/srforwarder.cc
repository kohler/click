/*
 * SRForwarder.{cc,hh} -- Source Route data path implementation
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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include "srforwarder.hh"
#include "srpacket.hh"
#include "srcrstat.hh"
#include "srcr.hh"
#include "elements/grid/arptable.hh"
CLICK_DECLS

#ifndef srforwarder_assert
#define srforwarder_assert(e) ((e) ? (void) 0 : srforwarder_assert_(__FILE__, __LINE__, #e))
#endif /* srforwarder_assert */


SRForwarder::SRForwarder()
  :  Element(1,3), 
     _ip(),
     _eth(),
     _et(0),
     _datas(0), 
     _databytes(0),
     _link_table(0),
     _arp_table(0),
     _srcr(0),
     _srcr_stat(0)

{
  MOD_INC_USE_COUNT;

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

SRForwarder::~SRForwarder()
{
  MOD_DEC_USE_COUNT;
}


int
SRForwarder::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int res;
  res = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "ETH", cpEtherAddress, "Ethernet Address", &_eth,
		    "ARP", cpElement, "ARPTable element", &_arp_table,
		    /* below not required */
		    "SS", cpElement, "SRCRStat element", &_srcr_stat,
		    "SRCR", cpElement, "SRCR element", &_srcr,
		    "LT", cpElement, "LinkTable element", &_link_table,
                    0);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_eth) 
    return errh->error("ETH not specified");
  if (!_arp_table) 
    return errh->error("ARPTable not specified");

  if (_arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not a ARPTable");

  if (_srcr_stat && _srcr_stat->cast("SrcrStat") == 0) 
    return errh->error("SS element is not a SRCRStat");
  if (_srcr && _srcr->cast("SRCR") == 0) 
    return errh->error("SRCR element is not a SRCR");
  if (_link_table && _link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");

  if (res < 0) {
    return res;
  }
  return res;
}

SRForwarder *
SRForwarder::clone () const
{
  return new SRForwarder;
}

int
SRForwarder::initialize (ErrorHandler *)
{
  return 0;
}

// Ask LinkStat for the metric for the link from other to us.
int
SRForwarder::get_metric(IPAddress other)
{
  int metric = 9999;
  srforwarder_assert(other);
  if (_srcr_stat) {
    metric = _srcr_stat->get_etx(other);
    update_link(_ip, other, metric);
    return metric;
    
  } else {
    return 0;
  }
}

void
SRForwarder::update_link(IPAddress from, IPAddress to, int metric) 
{
  if (_link_table) {
    _link_table->update_link(from, to, metric);
    _link_table->update_link(to, from, metric);
  }

}



void
SRForwarder::send(const u_char *payload, u_long payload_len, Vector<IPAddress> r, int flags)
{
  Packet *p_out = encap(payload, payload_len, r, flags);
  output(1).push(p_out);

}
Packet *
SRForwarder::encap(const u_char *payload, u_long payload_len, Vector<IPAddress> r, int flags)
{
  int hops = r.size();
  int len = srpacket::len_with_data(hops, payload_len);
  srforwarder_assert(r.size() > 1);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  memset(pk, '\0', len);

  memcpy(eh->ether_shost, _eth.data(), 6);
  EtherAddress eth_dest = _arp_table->lookup(r[1]);
  if (eth_dest == _arp_table->_bcast) {
    click_chatter("SRForwarder %s: arp lookup failed for %s",
		  id().cc(),
		  r[1].s().cc());
  }
  memcpy(eh->ether_dhost, eth_dest.data(), 6);
  eh->ether_type = htons(_et);
  pk->_version = _sr_version;
  pk->_type = PT_DATA;
  pk->_dlen = htons(payload_len);

  if (_srcr) {
    IPAddress neighbor = _srcr->get_random_neighbor();
    if (neighbor) {
      pk->set_random_from(_ip);
      pk->set_random_to(neighbor);
      pk->set_random_metric(get_metric(neighbor));
    }
  }
  pk->set_num_hops(r.size());
  pk->set_next(1);
  pk->set_flag(flags);
  int i;
  for(i = 0; i < hops; i++) {
    pk->set_hop(i, r[i]);
  }
  memcpy(pk->data(), payload, payload_len);


  /* set the ip header anno */
  const click_ip *ip = reinterpret_cast<const click_ip *>
    (pk->data());
  p->set_ip_header(ip, pk->data_len());

  return p;
}

void
SRForwarder::push(int port, Packet *p_in)
{
  if (port > 1) {
    p_in->kill();
    return;
  }
  click_ether *eh = (click_ether *) p_in->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);

  if(eh->ether_type != htons(_et)){
    click_chatter("SRForwarder %s: bad ether_type %04x",
                  _ip.s().cc(),
                  ntohs(eh->ether_type));
    p_in->kill();
    return;
  }

  if (pk->_type != PT_DATA) {
    click_chatter("SRForwarder %s: bad packet_type %04x",
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
      click_chatter("SRForwarder %s: data not for me %d/%d ip %s eth %s",
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
      click_chatter("SRForwarder %s: couldn't make packet\n", id().cc());
      p_in->kill();
      return;
    }
    memcpy(p_out->data(), pk->data(), pk->data_len());

    /* set the dst to the gateway it came from 
     * this is kinda weird.
     */
    SET_MISC_IP_ANNO(p_out, pk->get_hop(0));

    output(2).push(p_out);
    p_in->kill();
    return;
  } 

  int len = pk->hlen_with_data();
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0) {
    click_chatter("SRForwarder %s: couldn't make packet\n", id().cc());
    p_in->kill();
    return ;
  }
  click_ether *eh_out = (click_ether *) p->data();
  struct srpacket *pk_out = (struct srpacket *) (eh_out+1);
  memcpy(pk_out, pk, len);

  pk_out->set_metric(pk_out->next() - 1, prev_metric);
  pk_out->set_next(pk->next() + 1);
  pk_out->set_num_hops(pk->num_hops());
  eh_out->ether_type = htons(_et);

  srforwarder_assert(pk->next() < 8);
  IPAddress nxt = pk_out->get_hop(pk_out->next());
  
  /*
   * put new information in the random link field
   * with probability = 1/num_hops in the packet
   */
  if (_srcr && random() % pk_out->num_hops() == 0) {
    IPAddress r_neighbor = _srcr->get_random_neighbor();
    if (r_neighbor) {
      pk_out->set_random_from(_ip);
      pk_out->set_random_to(r_neighbor);
      pk_out->set_random_metric(get_metric(r_neighbor));
    }
  }


  EtherAddress eth_dest = _arp_table->lookup(nxt);
  if (eth_dest == _arp_table->_bcast) {
    click_chatter("SRForwarder %s: arp lookup failed for %s",
		  id().cc(),
		  nxt.s().cc());
  }
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
SRForwarder::static_print_stats(Element *f, void *)
{
  SRForwarder *d = (SRForwarder *) f;
  return d->print_stats();
}

String
SRForwarder::print_stats()
{
  
  return
    String(_datas) + " datas sent\n" +
    String(_databytes) + " bytes of data sent\n";

}

void
SRForwarder::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
}


void
SRForwarder::srforwarder_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("SRForwarder %s assertion \"%s\" failed: file %s, line %d",
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
EXPORT_ELEMENT(SRForwarder)
