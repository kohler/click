/*
 * fastudpsourceip6.{cc,hh} -- fast udp source, a benchmark tool
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/click_ip6.h>
#include "fastudpsrcip6.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
#ifdef CLICK_LINUXMODULE
# include <net/checksum.h>
#endif

const unsigned FastUDPSourceIP6::NO_LIMIT;

FastUDPSourceIP6::FastUDPSourceIP6()
  : _packet(0)
{
  _rate_limited = true;
  _first = _last = 0;
  _count = 0;
  add_output();
  MOD_INC_USE_COUNT;
}

FastUDPSourceIP6::~FastUDPSourceIP6()
{
  MOD_DEC_USE_COUNT;
}

int
FastUDPSourceIP6::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _cksum = true;
  _active = true;
  _interval = 0;
  unsigned sp, dp;
  unsigned rate;
  int limit;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "send rate", &rate,
		  cpInteger, "limit", &limit,
	      	  cpUnsigned, "packet length", &_len,
		  cpEthernetAddress, "src eth address", &_ethh.ether_shost,
		  cpIP6Address, "src ip6 address", &_sip6addr,
		  cpUnsigned, "src port", &sp,
		  cpEthernetAddress, "dst eth address", &_ethh.ether_dhost,
		  cpIP6Address, "dst ip6 address", &_dip6addr,
		  cpUnsigned, "dst port", &dp,
		  cpOptional,
		  cpBool, "do UDP checksum?", &_cksum,
		  cpUnsigned, "interval", &_interval,
		  cpBool, "active?", &_active,
		  0) < 0)
    return -1;
  if (sp >= 0x10000 || dp >= 0x10000)
    return errh->error("source or destination port too large");
  if (_len < 60) {
    click_chatter("warning: packet length < 60, defaulting to 60");
    _len = 60;
  }
  _ethh.ether_type = htons(0x86DD);
  _sport = sp;
  _dport = dp;
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
FastUDPSourceIP6::incr_ports()
{
  click_ip6 *ip6 = reinterpret_cast<click_ip6 *>(_packet->data()+14);
  click_udp *udp = reinterpret_cast<click_udp *>(ip6 + 1);
  _incr++;
  udp->uh_sport = htons(_sport+_incr);
  udp->uh_dport = htons(_dport+_incr);
  udp->uh_sum = 0;
  unsigned short len = _len-14-sizeof(click_ip6);
  if (_cksum) {   
    //need to chagne
    //unsigned csum = ~click_in_cksum((unsigned char *)udp, len) & 0xFFFF;
    //udp->uh_sum = csum_tcpudp_magic(_sip6addr.s_addr, _dip6addr.s_addr,
    //      		    len, IP_PROTO_UDP, csum); 
    udp->uh_sum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, udp->uh_sum, (unsigned char *)udp, ip6->ip6_plen));
  } else
    udp->uh_sum = 0;
}

int 
FastUDPSourceIP6::initialize(ErrorHandler *)
{
  _count = 0;
  _incr = 0;
  _packet = Packet::make(_len);
  memcpy(_packet->data(), &_ethh, 14);
  click_ip6 *ip6 = reinterpret_cast<click_ip6 *>(_packet->data()+14);
  click_udp *udp = reinterpret_cast<click_udp *>(ip6 + 1);
  
  // set up IP6 header
  ip6->ip6_flow = 0;
  ip6->ip6_v = 6;
  ip6->ip6_plen = htons(_len - 14 - sizeof(click_ip6));
  ip6->ip6_nxt = IP_PROTO_UDP;
  ip6->ip6_hlim = 250; 
  ip6->ip6_src = _sip6addr;
  ip6->ip6_dst = _dip6addr;
  _packet->set_dst_ip6_anno(IP6Address(_dip6addr));
  _packet->set_ip6_header(ip6, sizeof(click_ip6));

  // set up UDP header
  udp->uh_sport = htons(_sport);
  udp->uh_dport = htons(_dport);
  udp->uh_sum = 0;
  unsigned short len = _len-14-sizeof(click_ip6);
  udp->uh_ulen = htons(len);
  if (_cksum) {
    //need to change, use our own checksum method
    //unsigned csum = ~click_in_cksum((unsigned char *)udp, len) & 0xFFFF;
    //udp->uh_sum = csum_tcpudp_magic(_sipaddr.s_addr, _dipaddr.s_addr,
    //			    len, IP_PROTO_UDP, csum);
    udp->uh_sum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, udp->uh_sum, (unsigned char *)udp, ip6->ip6_plen));

  } else
    udp->uh_sum = 0;
    
  _skb = _packet->steal_skb();
  return 0;
}

void
FastUDPSourceIP6::cleanup(CleanupStage)
{
  if (_packet) {
    _packet->kill();
    _packet=0;
  }
}

Packet *
FastUDPSourceIP6::pull(int)
{
  Packet *p = 0;

  if (!_active || (_limit != NO_LIMIT && _count >= _limit)) return 0;

  if(_rate_limited){
    struct timeval now;
    click_gettimeofday(&now);
    if (_rate.need_update(now)) {
      _rate.update();
      atomic_inc(&_skb->users); 
      p = reinterpret_cast<Packet *>(_skb);
    }
  } else {
    atomic_inc(&_skb->users); 
    p = reinterpret_cast<Packet *>(_skb);
  }

  if(p) {
    assert(atomic_read(&_skb->users) > 1);
    _count++;
    if(_count == 1)
      _first = click_jiffies();
    if(_limit != NO_LIMIT && _count >= _limit)
      _last = click_jiffies();
    if(_interval>0 && !(_count%_interval))
      incr_ports();
  }

  return(p);
}

void
FastUDPSourceIP6::reset()
{
  _count = 0;
  _first = 0;
  _last = 0;
  _incr = 0;
}

static String
FastUDPSourceIP6_read_count_handler(Element *e, void *)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
  return String(c->count()) + "\n";
}

static String
FastUDPSourceIP6_read_rate_handler(Element *e, void *)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
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
FastUDPSourceIP6_reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
  c->reset();
  return 0;
}

static int
FastUDPSourceIP6_limit_write_handler
(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
  String s = cp_uncomment(in_s);
  unsigned limit;
  if (!cp_unsigned(s, &limit))
    return errh->error("limit parameter must be integer >= 0");
  c->_limit = (limit >= 0 ? limit : c->NO_LIMIT);
  return 0;
}

static int
FastUDPSourceIP6_rate_write_handler
(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
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
FastUDPSourceIP6_active_write_handler
(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
  String s = cp_uncomment(in_s);
  bool active;
  if (!cp_bool(s, &active)) 
    return errh->error("active parameter must be boolean");
  c->_active = active;
  if (active) c->reset();
  return 0;
}

void
FastUDPSourceIP6::add_handlers()
{
  add_read_handler("count", FastUDPSourceIP6_read_count_handler, 0);
  add_read_handler("rate", FastUDPSourceIP6_read_rate_handler, 0);
  add_write_handler("rate", FastUDPSourceIP6_rate_write_handler, 0);
  add_write_handler("reset", FastUDPSourceIP6_reset_write_handler, 0);
  add_write_handler("active", FastUDPSourceIP6_active_write_handler, 0);
  add_write_handler("limit", FastUDPSourceIP6_limit_write_handler, 0);
}

ELEMENT_REQUIRES(linuxmodule ip6)
EXPORT_ELEMENT(FastUDPSourceIP6)
