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
#include <clicknet/ip6.h>
#include "fastudpsrcip6.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>

const unsigned FastUDPSourceIP6::NO_LIMIT;

FastUDPSourceIP6::FastUDPSourceIP6()
  : _packet(0)
{
  _rate_limited = true;
  _first = _last = 0;
  _count = 0;
}

FastUDPSourceIP6::~FastUDPSourceIP6()
{
}

int
FastUDPSourceIP6::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _cksum = true;
  _active = true;
  _interval = 0;
  unsigned rate;
  int limit;
  if (Args(conf, this, errh)
      .read_mp("RATE", rate)
      .read_mp("LIMIT", limit)
      .read_mp("LENGTH", _len)
      .read_mp("SRCETH", EtherAddressArg(), _ethh.ether_shost)
      .read_mp("SRCIP6", _sip6addr)
      .read_mp("SPORT", IPPortArg(IP_PROTO_UDP), _sport)
      .read_mp("DSTETH", EtherAddressArg(), _ethh.ether_dhost)
      .read_mp("DSTIP6", _dip6addr)
      .read_mp("DPORT", IPPortArg(IP_PROTO_UDP), _dport)
      .read_p("CHECKSUM", _cksum)
      .read_p("INTERVAL", _interval)
      .read_p("ACTIVE", _active)
      .complete() < 0)
    return -1;
  if (_len < 60) {
    click_chatter("warning: packet length < 60, defaulting to 60");
    _len = 60;
  }
  _ethh.ether_type = htons(0x86DD);
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
  WritablePacket *q = _packet->uniqueify(); // better not fail
  _packet = q;
  click_ip6 *ip6 = reinterpret_cast<click_ip6 *>(q->data()+14);
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
    //		    len, IP_PROTO_UDP, csum);
    udp->uh_sum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, udp->uh_sum, (unsigned char *)udp, ip6->ip6_plen));
  } else
    udp->uh_sum = 0;
}

int
FastUDPSourceIP6::initialize(ErrorHandler *)
{
  _count = 0;
  _incr = 0;
  WritablePacket *q = Packet::make(_len);
  _packet = q;
  memcpy(q->data(), &_ethh, 14);
  click_ip6 *ip6 = reinterpret_cast<click_ip6 *>(q->data()+14);
  click_udp *udp = reinterpret_cast<click_udp *>(ip6 + 1);

  // set up IP6 header
  ip6->ip6_flow = 0;
  ip6->ip6_v = 6;
  ip6->ip6_plen = htons(_len - 14 - sizeof(click_ip6));
  ip6->ip6_nxt = IP_PROTO_UDP;
  ip6->ip6_hlim = 250;
  ip6->ip6_src = _sip6addr;
  ip6->ip6_dst = _dip6addr;
  SET_DST_IP6_ANNO(_packet, _dip6addr);
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
    if (_rate.need_update(Timestamp::now())) {
      _rate.update();
      p = _packet->clone();
    }
  } else {
    p = _packet->clone();
  }

  if(p) {
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
  return String(c->count());
}

static String
FastUDPSourceIP6_read_rate_handler(Element *e, void *)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
  if(c->last() != 0){
    int d = c->last() - c->first();
    if (d < 1) d = 1;
    int rate = c->count() * CLICK_HZ / d;
    return String(rate);
  } else {
    return String("0");
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
(const String &s, Element *e, void *, ErrorHandler *errh)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
  unsigned limit;
  if (!IntArg().parse(s, limit))
    return errh->error("limit parameter must be integer >= 0");
  c->_limit = (limit >= 0 ? limit : c->NO_LIMIT);
  return 0;
}

static int
FastUDPSourceIP6_rate_write_handler
(const String &s, Element *e, void *, ErrorHandler *errh)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
  unsigned rate;
  if (!IntArg().parse(s, rate))
    return errh->error("rate parameter must be integer >= 0");
  if (rate > GapRate::MAX_RATE)
    // report error rather than pin to max
    return errh->error("rate too large; max is %u", GapRate::MAX_RATE);
  c->_rate.set_rate(rate);
  return 0;
}

static int
FastUDPSourceIP6_active_write_handler
(const String &s, Element *e, void *, ErrorHandler *errh)
{
  FastUDPSourceIP6 *c = (FastUDPSourceIP6 *)e;
  bool active;
  if (!BoolArg().parse(s, active))
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
  add_write_handler("reset", FastUDPSourceIP6_reset_write_handler, 0, Handler::BUTTON);
  add_write_handler("active", FastUDPSourceIP6_active_write_handler, 0, Handler::CHECKBOX);
  add_write_handler("limit", FastUDPSourceIP6_limit_write_handler, 0);
}

ELEMENT_REQUIRES(ip6)
EXPORT_ELEMENT(FastUDPSourceIP6)
