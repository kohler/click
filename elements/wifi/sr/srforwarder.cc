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
#include "linkmetric.hh"
#include "elements/wifi/arptable.hh"
CLICK_DECLS


SRForwarder::SRForwarder()
  :  Element(1,2), 
     _ip(),
     _eth(),
     _et(0),
     _datas(0), 
     _databytes(0),
     _link_table(0),
     _arp_table(0),
     _metric(0)

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
		    "LM", cpElement, "LinkMetric element", &_metric,
		    "LT", cpElement, "LinkTable element", &_link_table,
                    cpEnd);

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

  if (_metric && _metric->cast("LinkMetric") == 0) 
    return errh->error("LinkMetric element is not a LinkMetric");
  if (_link_table && _link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");

  if (res < 0) {
    return res;
  }
  return res;
}

int
SRForwarder::initialize (ErrorHandler *)
{
  return 0;
}

int
SRForwarder::get_fwd_metric(IPAddress other)
{
  int metric = 0;
  sr_assert(other);
  if (_metric) {
    metric = _metric->get_fwd_metric(other);
    if (metric && !update_link(_ip, other, 0, metric)) {
      click_chatter("%{element} couldn't update fwd_metric %s > %d > %s\n",
		    this,
		    _ip.s().cc(),
		    metric,
		    other.s().cc());
    }
    return metric;
  } else {
    return 0;
  }
}


int
SRForwarder::get_rev_metric(IPAddress other)
{
  int metric = 0;
  sr_assert(other);
  if (_metric) {
    metric = _metric->get_rev_metric(other);
    if (metric && !update_link(other, _ip, 0, metric)) {
      click_chatter("%{element} couldn't update rev_metric %s > %d > %s\n",
		    this,
		    other.s().cc(),
		    metric,
		    _ip.s().cc());
    }
    return metric;
  } else {
    return 0;
  }
}

bool
SRForwarder::update_link(IPAddress from, IPAddress to, 
			 uint32_t seq, uint32_t metric) 
{
  if (_link_table && !_link_table->update_link(from, to, seq, metric)) {
    click_chatter("%{element} couldn't update link %s > %d > %s\n",
		  this,
		  from.s().cc(),
		  metric,
		  to.s().cc());
    return false;
  }
  return true;
}




Packet *
SRForwarder::encap(Packet *p_in, Vector<IPAddress> r, int flags)
{
  sr_assert(r.size() > 1);
  int hops = r.size() - 1;
  unsigned extra = srpacket::len_wo_data(hops) + sizeof(click_ether);
  unsigned payload_len = p_in->length();
  WritablePacket *p = p_in->push(extra);

  assert(extra + payload_len == p_in->length());


  int next = index_of(r, _ip) + 1;
  if (next < 0 || next >= r.size()) {
    click_chatter("SRForwarder %s: encap couldn't find %s (%d) in path %s",
		  id().cc(),
		  _ip.s().cc(),
		  next,
		  path_to_string(r).cc());
    p_in->kill();
    return (0);
  }
  EtherAddress eth_dest = _arp_table->lookup(r[next]);
  if (eth_dest == _arp_table->_bcast) {
    click_chatter("SRForwarder %s: arp lookup failed for %s",
		  id().cc(),
		  r[next].s().cc());
  }
  
  click_ether *eh = (click_ether *) p->data();
  memcpy(eh->ether_shost, _eth.data(), 6);
  memcpy(eh->ether_dhost, eth_dest.data(), 6);
  eh->ether_type = htons(_et);
    

  struct srpacket *pk = (struct srpacket *) (eh+1);
  memset(pk, '\0', srpacket::len_wo_data(hops));

  pk->_version = _sr_version;
  pk->_type = PT_DATA;
  pk->_dlen = htons(payload_len);

  pk->set_num_links(hops);
  pk->set_next(next);
  pk->set_flag(flags);
  int i;
  for(i = 0; i < hops; i++) {
    pk->set_link_node(i, r[i]);
  }

  pk->set_link_node(hops, r[r.size()-1]);

  PathInfo *nfo = _paths.findp(r);
  if (!nfo) {
    _paths.insert(r, PathInfo(r));
    nfo = _paths.findp(r);
  }
  pk->set_data_seq(nfo->_seq);
  
  click_gettimeofday(&nfo->_last_tx);
  nfo->_seq++;

  /* set the ip header anno */
  const click_ip *ip = reinterpret_cast<const click_ip *>
    (p->data() + pk->hlen_wo_data() + sizeof(click_ether));
  p->set_ip_header(ip, sizeof(click_ip));
  return p;
}

void
SRForwarder::push(int port, Packet *p_in)
{

  WritablePacket *p = p_in->uniqueify();

  if (!p) {
    return;
  }

  if (port > 1) {
    p->kill();
    return;
  }
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);

  if(eh->ether_type != htons(_et)){
    click_chatter("SRForwarder %s: bad ether_type %04x",
                  _ip.s().cc(),
                  ntohs(eh->ether_type));
    p->kill();
    return;
  }

  if (pk->_type != PT_DATA) {
    click_chatter("SRForwarder %s: bad packet_type %04x",
                  _ip.s().cc(),
                  pk->_type);
    p->kill();
    return ;
  }



  if(port == 0 && pk->get_link_node(pk->next()) != _ip){
    if (EtherAddress(eh->ether_dhost) != _bcast) {
      /* 
       * if the arp doesn't have a ethernet address, it
       * will broadcast the packet. in this case,
       * don't complain. But otherwise, something's up
       */
      click_chatter("SRForwarder %s: data not for me seq %d %d/%d ip %s eth %s",
		    id().cc(),
		    pk->data_seq(),
		    pk->next(),
		    pk->num_links(),
		    pk->get_link_node(pk->next()).s().cc(),
		    EtherAddress(eh->ether_dhost).s().cc());
    }
    p->kill();
    return;
  }

  /* update the metrics from the packet */
  IPAddress r_from = pk->get_random_from();
  IPAddress r_to = pk->get_random_to();
  int r_fwd_metric = pk->get_random_fwd_metric();
  int r_rev_metric = pk->get_random_rev_metric();
  uint32_t seq = pk->get_random_seq();
  if (r_from && r_to) {
    if (r_fwd_metric && !update_link(r_from, r_to, seq, r_fwd_metric)) {
      click_chatter("%{element} couldn't update r_fwd %s > %d > %s\n",
		    this,
		    r_from.s().cc(),
		    r_fwd_metric,
		    r_to.s().cc());
    }
    if (r_rev_metric && !update_link(r_to, r_from, seq, r_rev_metric)) {
      click_chatter("%{element} couldn't update r_rev %s > %d > %s\n",
		    this,
		    r_to.s().cc(),
		    r_rev_metric, 
		    r_from.s().cc());
    }
  }

  for(int i = 0; i < pk->num_links(); i++) {
    IPAddress a = pk->get_link_node(i);
    IPAddress b = pk->get_link_node(i+1);
    int fwd_m = pk->get_link_fwd(i);
    int rev_m = pk->get_link_rev(i);
    if (a != _ip && b != _ip) {
      /* don't update my immediate neighbor. see below */
      if (fwd_m && !update_link(a,b,0,fwd_m)) {
	click_chatter("%{element} couldn't update fwd_m %s > %d > %s\n",
		      this,
		      a.s().cc(),
		      fwd_m,
		      b.s().cc());
      }
      if (rev_m && !update_link(b,a,0,rev_m)) {
	click_chatter("%{element} couldn't update rev_m %s > %d > %s\n",
		      this,
		      b.s().cc(),
		      rev_m,
		      a.s().cc());
      }
    }
  }
  

  IPAddress prev = pk->get_link_node(pk->next()-1);
  _arp_table->insert(prev, EtherAddress(eh->ether_shost));


  /* set the ip header anno */
  const click_ip *ip = reinterpret_cast<const click_ip *>
    (pk->data());
  p->set_ip_header(ip, sizeof(click_ip));
  
  /* 
   * these functions also update the link
   * table, so we don't need to call update_link
   */

  uint32_t prev_fwd_metric = get_fwd_metric(prev);
  uint32_t prev_rev_metric = get_rev_metric(prev);

  if(pk->next() == pk->num_links()){
    // I'm the ultimate consumer of this data.
    /*
     * set the dst to the gateway it came from 
     * this is kinda weird.
     */
    SET_MISC_IP_ANNO(p, pk->get_link_node(0));
    output(1).push(p);
    return;
  } 

  pk->set_link(pk->next()-1,
	       pk->get_link_node(pk->next()-1), _ip,
	       prev_fwd_metric, prev_rev_metric,
	       0,0);

  pk->set_next(pk->next() + 1);
  IPAddress nxt = pk->get_link_node(pk->next());
  sr_assert(pk->next() < 8);

  
  EtherAddress eth_dest = _arp_table->lookup(nxt);
  if (eth_dest == _arp_table->_bcast) {
    click_chatter("%{element}::%s arp lookup failed for %s",
		  this,
		  __func__,
		  nxt.s().cc());
  }
  memcpy(eh->ether_dhost, eth_dest.data(), 6);
  memcpy(eh->ether_shost, _eth.data(), 6);

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



// generate Vector template instance
#include <click/vector.cc>
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES

#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SRForwarder)
