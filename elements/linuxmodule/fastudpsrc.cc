/*
 * fastudpsource.{cc,hh} -- fast udp source, a benchmark tool
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
#include <clicknet/ip.h>
#include "fastudpsrc.hh"
#include <click/args.hh>
#include <click/etheraddress.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>

const unsigned FastUDPSource::NO_LIMIT;

FastUDPSource::FastUDPSource()
  : _packet(0)
{
  _rate_limited = true;
  _first = _last = 0;
  _count = 0;
}

FastUDPSource::~FastUDPSource()
{
}

int
FastUDPSource::configure(Vector<String> &conf, ErrorHandler *errh)
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
      .read_mp("SRCIP", _sipaddr)
      .read_mp("SPORT", IPPortArg(IP_PROTO_UDP), _sport)
      .read_mp("DSTETH", EtherAddressArg(), _ethh.ether_dhost)
      .read_mp("DSTIP", _dipaddr)
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
FastUDPSource::incr_ports()
{
  WritablePacket *q = _packet->uniqueify(); // better not fail
  _packet = q;
  click_ip *ip = reinterpret_cast<click_ip *>(q->data()+14);
  click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);
  _incr++;
  udp->uh_sport = htons(_sport+_incr);
  udp->uh_dport = htons(_dport+_incr);
  udp->uh_sum = 0;
  unsigned short len = _len-14-sizeof(click_ip);
  if (_cksum) {
    unsigned csum = ~click_in_cksum((unsigned char *)udp, len) & 0xFFFF;
    udp->uh_sum = csum_tcpudp_magic(_sipaddr.s_addr, _dipaddr.s_addr,
				    len, IP_PROTO_UDP, csum);
  } else
    udp->uh_sum = 0;
}

int
FastUDPSource::initialize(ErrorHandler *)
{
  _count = 0;
  _incr = 0;
  WritablePacket *q = Packet::make(_len);
  _packet = q;
  memcpy(q->data(), &_ethh, 14);
  click_ip *ip = reinterpret_cast<click_ip *>(q->data()+14);
  click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

  // set up IP header
  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(_len-14);
  ip->ip_id = 0;
  ip->ip_p = IP_PROTO_UDP;
  ip->ip_src = _sipaddr;
  ip->ip_dst = _dipaddr;
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 250;
  ip->ip_sum = 0;
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
  _packet->set_dst_ip_anno(IPAddress(_dipaddr));
  _packet->set_ip_header(ip, sizeof(click_ip));

  // set up UDP header
  udp->uh_sport = htons(_sport);
  udp->uh_dport = htons(_dport);
  udp->uh_sum = 0;
  unsigned short len = _len-14-sizeof(click_ip);
  udp->uh_ulen = htons(len);
  if (_cksum) {
    unsigned csum = ~click_in_cksum((unsigned char *)udp, len) & 0xFFFF;
    udp->uh_sum = csum_tcpudp_magic(_sipaddr.s_addr, _dipaddr.s_addr,
				    len, IP_PROTO_UDP, csum);
  } else
    udp->uh_sum = 0;

  return 0;
}

void
FastUDPSource::cleanup(CleanupStage)
{
  if (_packet) {
    _packet->kill();
    _packet=0;
  }
}

Packet *
FastUDPSource::pull(int)
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
FastUDPSource::reset()
{
  _count = 0;
  _first = 0;
  _last = 0;
  _incr = 0;
}

static String
FastUDPSource_read_count_handler(Element *e, void *)
{
  FastUDPSource *c = (FastUDPSource *)e;
  return String(c->count());
}

static String
FastUDPSource_read_rate_handler(Element *e, void *)
{
  FastUDPSource *c = (FastUDPSource *)e;
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
FastUDPSource_reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  FastUDPSource *c = (FastUDPSource *)e;
  c->reset();
  return 0;
}

static int
FastUDPSource_limit_write_handler
(const String &s, Element *e, void *, ErrorHandler *errh)
{
  FastUDPSource *c = (FastUDPSource *)e;
  unsigned limit;
  if (!IntArg().parse(s, limit))
    return errh->error("limit parameter must be integer >= 0");
  c->_limit = (limit >= 0 ? limit : c->NO_LIMIT);
  return 0;
}

static int
FastUDPSource_rate_write_handler
(const String &s, Element *e, void *, ErrorHandler *errh)
{
  FastUDPSource *c = (FastUDPSource *)e;
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
FastUDPSource_active_write_handler
(const String &s, Element *e, void *, ErrorHandler *errh)
{
  FastUDPSource *c = (FastUDPSource *)e;
  bool active;
  if (!BoolArg().parse(s, active))
    return errh->error("active parameter must be boolean");
  c->_active = active;
  if (active) c->reset();
  return 0;
}

void
FastUDPSource::add_handlers()
{
  add_read_handler("count", FastUDPSource_read_count_handler, 0);
  add_read_handler("rate", FastUDPSource_read_rate_handler, 0);
  add_write_handler("rate", FastUDPSource_rate_write_handler, 0);
  add_write_handler("reset", FastUDPSource_reset_write_handler, 0, Handler::BUTTON);
  add_write_handler("active", FastUDPSource_active_write_handler, 0, Handler::CHECKBOX);
  add_write_handler("limit", FastUDPSource_limit_write_handler, 0);
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(FastUDPSource)
