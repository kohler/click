/*
 * fasttcpflows.{cc,hh} -- fast tcp flow source, a benchmark tool
 * Benjie Chen
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
#include <click/click_ip.h>
#include "fasttcpflows.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "elements/standard/alignmentinfo.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

FastTCPFlows::FastTCPFlows()
  : _flows(0)
{
  _rate_limited = true;
  _first = _last = 0;
  _count = 0;
  add_output();
  MOD_INC_USE_COUNT;
}

FastTCPFlows::~FastTCPFlows()
{
  uninitialize();
  MOD_DEC_USE_COUNT;
}

int
FastTCPFlows::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _cksum = true;
  _active = true;
  unsigned rate;
  int limit;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "send rate", &rate,
		  cpInteger, "limit", &limit,
	      	  cpUnsigned, "packet length", &_len,
		  cpEthernetAddress, "src eth address", &_ethh.ether_shost,
		  cpIPAddress, "src ip address", &_sipaddr,
		  cpEthernetAddress, "dst eth address", &_ethh.ether_dhost,
		  cpIPAddress, "dst ip address", &_dipaddr,
		  cpUnsigned, "number of flows", &_nflows,
		  cpUnsigned, "flow size", &_flowsize,
		  cpOptional,
		  cpBool, "active?", &_active,
		  0) < 0)
    return -1;
  if (_flowsize < 3) {
    click_chatter("warning: flow size < 3, defaulting to 3");
    _flowsize = 3;
  }
  if (_len < 60) {
    click_chatter("warning: packet length < 60, defaulting to 60");
    _len = 60;
  }
  _ethh.ether_type = htons(0x0800);
  if(rate != 0){
    _rate_limited = true;
    _rate.set_rate(rate, errh);
  } else {
    _rate_limited = false;
  }
  _limit = (limit >= 0 ? limit : NO_LIMIT);
  return 0;
}

void
FastTCPFlows::change_ports(int flow)
{
  unsigned short sport = (random() >> 2) % 0xFFFF;
  unsigned short dport = (random() >> 2) % 0xFFFF;
  click_ip *ip = 
    reinterpret_cast<click_ip *>(_flows[flow].syn_packet->data()+14);
  click_tcp *tcp = reinterpret_cast<click_tcp *>(ip + 1);
  tcp->th_sport = sport;
  tcp->th_dport = dport;
  tcp->th_sum = 0;
  unsigned short len = _len-14-sizeof(click_ip);
  unsigned csum = ~click_in_cksum((unsigned char *)tcp, len) & 0xFFFF;
  tcp->th_sum = csum_tcpudp_magic
    (_sipaddr.s_addr, _dipaddr.s_addr, len, IP_PROTO_TCP, csum);
  
  ip = reinterpret_cast<click_ip *>(_flows[flow].data_packet->data()+14);
  tcp = reinterpret_cast<click_tcp *>(ip + 1);
  tcp->th_sport = sport;
  tcp->th_dport = dport;
  tcp->th_sum = 0;
  len = _len-14-sizeof(click_ip);
  csum = ~click_in_cksum((unsigned char *)tcp, len) & 0xFFFF;
  tcp->th_sum = csum_tcpudp_magic
    (_sipaddr.s_addr, _dipaddr.s_addr, len, IP_PROTO_TCP, csum);
  
  ip = reinterpret_cast<click_ip *>(_flows[flow].fin_packet->data()+14);
  tcp = reinterpret_cast<click_tcp *>(ip + 1);
  tcp->th_sport = sport;
  tcp->th_dport = dport;
  tcp->th_sum = 0;
  len = _len-14-sizeof(click_ip);
  csum = ~click_in_cksum((unsigned char *)tcp, len) & 0xFFFF;
  tcp->th_sum = csum_tcpudp_magic
    (_sipaddr.s_addr, _dipaddr.s_addr, len, IP_PROTO_TCP, csum);
}

Packet *
FastTCPFlows::get_packet()
{
  if (_limit != NO_LIMIT && _count >= _limit) {
    for (unsigned i=0; i<_nflows; i++) {
      if (_flows[i].flow_count != _flowsize) {
	_flows[i].flow_count = _flowsize;
        atomic_inc(&(_flows[i].fin_packet->steal_skb())->users);
        return reinterpret_cast<Packet *>(_flows[i].fin_packet->steal_skb());
      }
    }
    _sent_all_fins = true;
    return 0;
  }
  else {
    int flow = (random() >> 2) % _nflows;
    if (_flows[flow].flow_count == _flowsize) {
      change_ports(flow);
      _flows[flow].flow_count = 0;
    }
    _flows[flow].flow_count++;
    if (_flows[flow].flow_count == 1) {
      atomic_inc(&(_flows[flow].syn_packet->steal_skb())->users);
      return reinterpret_cast<Packet *>(_flows[flow].syn_packet->steal_skb());
    }
    else if (_flows[flow].flow_count == _flowsize) {
      atomic_inc(&(_flows[flow].fin_packet->steal_skb())->users);
      return reinterpret_cast<Packet *>(_flows[flow].fin_packet->steal_skb());
    }
    else {
      atomic_inc(&(_flows[flow].data_packet->steal_skb())->users);
      return reinterpret_cast<Packet *>(_flows[flow].data_packet->steal_skb());
    }
  }
}
  

int 
FastTCPFlows::initialize(ErrorHandler *)
{
  _count = 0;
  _sent_all_fins = false;
  _flows = new flow_t[_nflows];

  for (int i=0; i<_nflows; i++) {
    unsigned short sport = (random() >> 2) % 0xFFFF;
    unsigned short dport = (random() >> 2) % 0xFFFF;
   
    // SYN packet
    _flows[i].syn_packet = Packet::make(_len);
    memcpy(_flows[i].syn_packet->data(), &_ethh, 14);
    click_ip *ip = 
      reinterpret_cast<click_ip *>(_flows[i].syn_packet->data()+14);
    click_tcp *tcp = reinterpret_cast<click_tcp *>(ip + 1);
    // set up IP header
    ip->ip_v = 4;
    ip->ip_hl = sizeof(click_ip) >> 2;
    ip->ip_len = htons(_len-14);
    ip->ip_id = 0;
    ip->ip_p = IP_PROTO_TCP;
    ip->ip_src = _sipaddr;
    ip->ip_dst = _dipaddr;
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 250;
    ip->ip_sum = 0;
    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
    _flows[i].syn_packet->set_dst_ip_anno(IPAddress(_dipaddr));
    _flows[i].syn_packet->set_ip_header(ip, sizeof(click_ip));
    // set up TCP header
    tcp->th_sport = sport;
    tcp->th_dport = dport;
    tcp->th_seq = random();
    tcp->th_ack = random();
    tcp->th_off = sizeof(click_tcp) >> 2;
    tcp->th_flags = TH_SYN;
    tcp->th_win = 65535;
    tcp->th_urp = 0;
    tcp->th_sum = 0;
    unsigned short len = _len-14-sizeof(click_ip);
    unsigned csum = ~click_in_cksum((unsigned char *)tcp, len) & 0xFFFF;
    tcp->th_sum = csum_tcpudp_magic(_sipaddr.s_addr, _dipaddr.s_addr,
				    len, IP_PROTO_TCP, csum);
    
    // DATA packet with PUSH and ACK
    _flows[i].data_packet = Packet::make(_len);
    memcpy(_flows[i].data_packet->data(), &_ethh, 14);
    ip = reinterpret_cast<click_ip *>(_flows[i].data_packet->data()+14);
    tcp = reinterpret_cast<click_tcp *>(ip + 1);
    // set up IP header
    ip->ip_v = 4;
    ip->ip_hl = sizeof(click_ip) >> 2;
    ip->ip_len = htons(_len-14);
    ip->ip_id = 0;
    ip->ip_p = IP_PROTO_TCP;
    ip->ip_src = _sipaddr;
    ip->ip_dst = _dipaddr;
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 250;
    ip->ip_sum = 0;
    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
    _flows[i].data_packet->set_dst_ip_anno(IPAddress(_dipaddr));
    _flows[i].data_packet->set_ip_header(ip, sizeof(click_ip));
    // set up TCP header
    tcp->th_sport = sport;
    tcp->th_dport = dport;
    tcp->th_seq = random();
    tcp->th_ack = random();
    tcp->th_off = sizeof(click_tcp) >> 2;
    tcp->th_flags = TH_PUSH | TH_ACK;
    tcp->th_win = 65535;
    tcp->th_urp = 0;
    tcp->th_sum = 0;
    len = _len-14-sizeof(click_ip);
    csum = ~click_in_cksum((unsigned char *)tcp, len) & 0xFFFF;
    tcp->th_sum = csum_tcpudp_magic(_sipaddr.s_addr, _dipaddr.s_addr,
				    len, IP_PROTO_TCP, csum);
  
    // FIN packet
    _flows[i].fin_packet = Packet::make(_len);
    memcpy(_flows[i].fin_packet->data(), &_ethh, 14);
    ip = reinterpret_cast<click_ip *>(_flows[i].fin_packet->data()+14);
    tcp = reinterpret_cast<click_tcp *>(ip + 1);
    // set up IP header
    ip->ip_v = 4;
    ip->ip_hl = sizeof(click_ip) >> 2;
    ip->ip_len = htons(_len-14);
    ip->ip_id = 0;
    ip->ip_p = IP_PROTO_TCP;
    ip->ip_src = _sipaddr;
    ip->ip_dst = _dipaddr;
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 250;
    ip->ip_sum = 0;
    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
    _flows[i].fin_packet->set_dst_ip_anno(IPAddress(_dipaddr));
    _flows[i].fin_packet->set_ip_header(ip, sizeof(click_ip));
    // set up TCP header
    tcp->th_sport = sport;
    tcp->th_dport = dport;
    tcp->th_seq = random();
    tcp->th_ack = random();
    tcp->th_off = sizeof(click_tcp) >> 2;
    tcp->th_flags = TH_FIN;
    tcp->th_win = 65535;
    tcp->th_urp = 0;
    tcp->th_sum = 0;
    len = _len-14-sizeof(click_ip);
    csum = ~click_in_cksum((unsigned char *)tcp, len) & 0xFFFF;
    tcp->th_sum = csum_tcpudp_magic(_sipaddr.s_addr, _dipaddr.s_addr,
				    len, IP_PROTO_TCP, csum);
    
    _flows[i].flow_count = 0;
  }
  _last_flow = 0;
  return 0;
}

void
FastTCPFlows::uninitialize()
{
  if (_flows) {
    for (int i=0; i<_nflows; i++) {
      _flows[i].syn_packet->kill();
      _flows[i].data_packet->kill();
      _flows[i].fin_packet->kill();
    }
    delete[] _flows;
    _flows = 0;
  }
}

Packet *
FastTCPFlows::pull(int)
{
  Packet *p = 0;

  if (!_active)
    return 0;
  if (_limit != NO_LIMIT && _count >= _limit && _sent_all_fins)
    return 0;

  if(_rate_limited){
    struct timeval now;
    click_gettimeofday(&now);
    if (_rate.need_update(now)) {
      _rate.update();
      p = get_packet();
    }
  } else
    p = get_packet();

  if(p) {
    assert(_skb->users > 1);
    _count++;
    if(_count == 1)
      _first = click_jiffies();
    if(_limit != NO_LIMIT && _count >= _limit)
      _last = click_jiffies();
  }

  return(p);
}

void
FastTCPFlows::reset()
{
  _count = 0;
  _first = 0;
  _last = 0;
  _sent_all_fins = false;
}

static String
FastTCPFlows_read_count_handler(Element *e, void *)
{
  FastTCPFlows *c = (FastTCPFlows *)e;
  return String(c->count()) + "\n";
}

static String
FastTCPFlows_read_rate_handler(Element *e, void *)
{
  FastTCPFlows *c = (FastTCPFlows *)e;
  if(c->last() != 0){
    int d = c->last() - c->first();
    if (d < 1) d = 1;
    int rate = c->count() * CLICK_HZ / d;
    return String(rate) + "\n";
  } else {
    return String("0\n");
  }
}

static int
FastTCPFlows_reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  FastTCPFlows *c = (FastTCPFlows *)e;
  c->reset();
  return 0;
}

static int
FastTCPFlows_limit_write_handler
(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  FastTCPFlows *c = (FastTCPFlows *)e;
  String s = cp_uncomment(in_s);
  unsigned limit;
  if (!cp_unsigned(s, &limit))
    return errh->error("limit parameter must be integer >= 0");
  c->_limit = (limit >= 0 ? limit : c->NO_LIMIT);
  return 0;
}

static int
FastTCPFlows_rate_write_handler
(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  FastTCPFlows *c = (FastTCPFlows *)e;
  String s = cp_uncomment(in_s);
  unsigned rate;
  if (!cp_unsigned(s, &rate))
    return errh->error("rate parameter must be integer >= 0");
  if (rate > GapRate::MAX_RATE)
    // report error rather than pin to max
    return errh->error("rate too large; max is %u", GapRate::MAX_RATE);
  c->_rate.set_rate(rate);
  return 0;
}

static int
FastTCPFlows_active_write_handler
(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  FastTCPFlows *c = (FastTCPFlows *)e;
  String s = cp_uncomment(in_s);
  bool active;
  if (!cp_bool(s, &active)) 
    return errh->error("active parameter must be boolean");
  c->_active = active;
  if (active) c->reset();
  return 0;
}

void
FastTCPFlows::add_handlers()
{
  add_read_handler("count", FastTCPFlows_read_count_handler, 0);
  add_read_handler("rate", FastTCPFlows_read_rate_handler, 0);
  add_write_handler("rate", FastTCPFlows_rate_write_handler, 0);
  add_write_handler("reset", FastTCPFlows_reset_write_handler, 0);
  add_write_handler("active", FastTCPFlows_active_write_handler, 0);
  add_write_handler("limit", FastTCPFlows_limit_write_handler, 0);
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(FastTCPFlows)

