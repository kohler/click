/*
 * dsrarptable.{cc,hh}
 * Shweta Bhandare, Sagar Sanghani, Sheetalkumar Doshi, Timothy X Brown
 * Daniel Aguayo
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 * Copyright (c) 2003 University of Colorado at Boulder
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
#include <click/package.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include "dsrarptable.hh"
#include "dsr.hh"

CLICK_DECLS
#define DEBUG_CHATTER  if (_debug) click_chatter

DSRArpTable::DSRArpTable()
  : _debug(false)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_input();
  add_input();
  add_output();
  add_output();
  add_output();
}

DSRArpTable::~DSRArpTable()
{
  MOD_DEC_USE_COUNT;
}

DSRArpTable *
DSRArpTable::clone() const
{
  return new DSRArpTable;
}

int
DSRArpTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  unsigned int etht = 0x0800;
  if (cp_va_parse(conf, this, errh, 
		  cpIPAddress, "ip address", &_me,
		  cpEthernetAddress,"ethernet address", &_me_ether,
                  cpKeywords,
                  "ETHERTYPE", cpUnsigned, "Ethernet encapsulation type", &etht,
		  "DEBUG", cpBool, "Debug", &_debug,
		  0) < 0)
    return -1;

  _etht = etht;

  return 0;
}

int
DSRArpTable::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
DSRArpTable::pull(int port)
{
  assert(port == 0 || port == 1);

  // packets to which we want to add an ethernet header
  Packet *p_in = input(port).pull();
  if (!p_in)
    return NULL;

  WritablePacket *q = p_in->push(sizeof(click_ether));
  if (!q) {
    click_chatter("DSRArpTable::pull(%d):  could not push space for ethernet header\n", port);
    return NULL;
  }
  
  click_ether *ether = (click_ether *)(q->data());
    
  IPAddress dst_addr(q->dst_ip_anno());
  EtherAddress dst_ether = lookup_ip(dst_addr);
    
  if (! dst_ether) {
    click_chatter ("DSRArpTable::push:  missing ARP table entry!  src: %s/%s dst: %s/%s\n", 
		   _me.s().cc(), _me_ether.s().cc(), 
		   dst_addr.s().cc(), dst_ether.s().cc());
    q->kill();
    return NULL;
  }
    
  memcpy(&ether->ether_shost, &_me_ether, 6);
  memcpy(&ether->ether_dhost, dst_ether.data(), 6);
  ether->ether_type = htons(_etht);
    
  return q;
}

void
DSRArpTable::push(int port, Packet *p_in)
{
  // packets from which we want to extract MAC addresses

  assert(port == 2); 
  
  const click_ip *iph = (const click_ip*)(p_in->data() + sizeof(click_ether));
  
  if (iph->ip_p != IP_PROTO_DSR) {
    DEBUG_CHATTER ("DSRArpTable::push:  non-DSR packet passing through DSRArpTable\n");
    output(2).push(p_in);
    return;
  }

  const click_ether *eth = (const click_ether *)p_in->data();
  EtherAddress mac = EtherAddress(eth->ether_shost);
  IPAddress ip = last_hop_ip(p_in);

  EtherAddress e = lookup_ip(ip);
  if (!e) {
    DEBUG_CHATTER("DSRArpTable::push:  adding ARP table entry for IP: %s; MAC: %s", 
		  ip.s().cc(), mac.s().cc());
    add_entry(ip, mac);
  } else if (e != mac) {
    click_chatter("DSRArpTable::push:  existing entry for %s has different MAC!  %s, not %s", 
		  ip.s().cc(), e.s().cc(), mac.s().cc());
    delete_entry(ip);
    add_entry(ip, mac);
  }

  WritablePacket *p = p_in->uniqueify();
  SET_DSR_LAST_HOP_IP_ANNO(p, ip.addr());

  const uint16_t *d = e.sdata();
  SET_DSR_LAST_HOP_ETH_ANNO1(p, d[0]);
  SET_DSR_LAST_HOP_ETH_ANNO2(p, d[1]);
  SET_DSR_LAST_HOP_ETH_ANNO3(p, d[2]);

  output(2).push(p);
}

// returns the ip of the last forwarder of this packet.  if this is a
// route request, this comes from the accumulated route.  otherwise it
// comes from the IP header (src and dst) and source route.  if there
// is no source route option, a one-hop source route is implied.

IPAddress
DSRArpTable::last_hop_ip(Packet *p)
{
  const click_ip *ip = (const click_ip *)(p->data() + sizeof(click_ether));

  click_dsr *dsr = (click_dsr *)((char *)ip + sizeof(click_ip));
  const unsigned int dsr_len = ntohs(dsr->dsr_len);

  click_dsr_option *dsr_option = (click_dsr_option *)((char *)dsr +
						      sizeof(click_dsr));
  
//   click_chatter("last_hop_ip: dsr len is %d; first type is %x\n",
// 		dsr_len, dsr_option->dsr_type);
  
  if (dsr_option->dsr_type == DSR_TYPE_RREQ) {

    const click_dsr_rreq *dsr_rreq = (click_dsr_rreq *)dsr_option;
    const unsigned int num_addr = dsr_rreq->num_addrs();
    
    if (num_addr == 0) { // this route request is on its first hop
      IPAddress src_addr(ip->ip_src.s_addr);
      return (src_addr);
    } else {
      IPAddress last_hop = dsr_rreq->addr[num_addr-1].ip();
      DEBUG_CHATTER ("saw a route request being forwarded by %s\n",
		     last_hop.s().cc());
      return (last_hop);
    }

  } else if (dsr_option->dsr_type == DSR_TYPE_RREP) {
    
    // if this is a route reply, then we expect a source route option
    // immediately following -- move the dsr_option pointer down to
    // the next header and handle as a normal source-routed packet.
    // if there is no source route option following, then this must be
    // a one-hop reply.
    
    const click_dsr_rrep *dsr_rrep = (click_dsr_rrep *)dsr_option;
    
    //    DEBUG_CHATTER(" * extracting IP from route reply; num_addr is %d\n", num_addr);

    if (dsr_rrep->length() == dsr_len) {
      // if this RREP option is the only option in the header
      IPAddress src_addr(ip->ip_src.s_addr);
      return (src_addr);
    } else {
      dsr_option = (click_dsr_option *)(dsr_rrep->next_option());
      
      if (dsr_option->dsr_type != DSR_TYPE_SOURCE_ROUTE) {
	DEBUG_CHATTER(" * DSRArpTable::last_hop: source route option did not follow route reply option (%x)\n", 
		      dsr_option->dsr_type);
	
	IPAddress zeros;
	return zeros;
      }
    }

  } else if (dsr_option->dsr_type == DSR_TYPE_RERR) {
    
    // if this is a route error, then we expect a source route option
    // immediately following -- handle as we did with RREP.  if there
    // is no source route option following, then this must be a
    // one-hop reply.
    //
    // XXX we might have multiple RERRs.
    
    const click_dsr_rerr *dsr_rerr = (click_dsr_rerr *)dsr_option;

    if (dsr_rerr->length() == dsr_len) {
      // if this RERR option is the only option in the header
      IPAddress src_addr(ip->ip_src.s_addr);
      return (src_addr);
    } else {
      dsr_option = (click_dsr_option *)(dsr_rerr->next_option());
      
      if (dsr_option->dsr_type != DSR_TYPE_SOURCE_ROUTE) {
	DEBUG_CHATTER(" * DSRArpTable::last_hop: source route option did not follow route error option (%x)\n", 
		      dsr_option->dsr_type);
	
	IPAddress zeros;
	return zeros;
      }
    }
  }
  
  if (dsr_option->dsr_type == DSR_TYPE_SOURCE_ROUTE) {
    // either this is a normal source-routed packet, or a RREP or RERR
    // with a source route header

    click_dsr_source *dsr_source = (click_dsr_source *)(dsr_option);
    assert(dsr_source->dsr_type == DSR_TYPE_SOURCE_ROUTE);
    
    unsigned char segments = dsr_source->dsr_segsleft;
    unsigned char source_hops = dsr_source->num_addrs();
    
    assert(segments <= source_hops);
    
    int index = source_hops - segments;
    
    if (index == 0) { // this is the first hop
      IPAddress src(ip->ip_src.s_addr);
      return src;
    } else {
      return dsr_source->addr[index-1].ip();
    }
  }

  assert(0);
  return IPAddress();
}

void 
DSRArpTable::add_entry(IPAddress ip, EtherAddress eth)
{
  EtherAddress *e =_ip_map.findp(ip);
  assert(!e);
  _ip_map.insert(ip, eth);
}

void 
DSRArpTable::delete_entry(IPAddress ip)
{
  EtherAddress *e =_ip_map.findp(ip);
  assert(e);
  _ip_map.remove(ip);
}

EtherAddress 
DSRArpTable::lookup_ip(IPAddress ip)
{
  IPAddress bcast_ip("255.255.255.255");

  if (ip == bcast_ip) {
    EtherAddress bcast_eth((unsigned char *)("\xff\xff\xff\xff\xff\xff"));
    return bcast_eth;
  }
  
  return (_ip_map.find(ip));
}

EXPORT_ELEMENT(DSRArpTable)

#include <click/bighashmap.cc>
template class HashMap<IPAddress, EtherAddress>;

CLICK_ENDDECLS
