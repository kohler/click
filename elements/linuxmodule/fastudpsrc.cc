/*
 * fastudpsource.{cc,hh} -- fast udp source, a benchmark tool
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include <click/click_ip.h>
#include "fastudpsrc.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "elements/standard/alignmentinfo.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

FastUDPSource::FastUDPSource()
  : _packet(0)
{
  _rate_limited = true;
  _first = _last = 0;
  _count = 0;
  add_output();
  MOD_INC_USE_COUNT;
}

FastUDPSource::~FastUDPSource()
{
  uninitialize();
  MOD_DEC_USE_COUNT;
}

int
FastUDPSource::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  bool _cksum = true;
  unsigned sp, dp;
  unsigned rate;
  int limit;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "send rate", &rate,
		  cpInteger, "limit", &limit,
	      	  cpUnsigned, "packet length", &_len,
		  cpEthernetAddress, "src eth address", &_ethh.ether_shost,
		  cpIPAddress, "src ip address", &_sipaddr,
		  cpUnsigned, "src port", &sp,
		  cpEthernetAddress, "dst eth address", &_ethh.ether_dhost,
		  cpIPAddress, "dst ip address", &_dipaddr,
		  cpUnsigned, "dst port", &dp,
		  cpOptional,
		  cpBool, "do UDP checksum?", &_cksum,
		  0) < 0)
    return -1;
  if (sp >= 0x10000 || dp >= 0x10000)
    return errh->error("source or destination port too large");
  if (_len < 60) {
    click_chatter("warning: packet length < 60, defaulting to 60");
    _len = 60;
  }
  _ethh.ether_type = htons(0x0800);
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

int 
FastUDPSource::initialize(ErrorHandler *)
{
  _count = 0;
  _packet = Packet::make(_len);
  memcpy(_packet->data(), &_ethh, 14);
  click_ip *ip = reinterpret_cast<click_ip *>(_packet->data()+14);
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
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));
  _packet->set_dst_ip_anno(IPAddress(_dipaddr));
  _packet->set_ip_header(ip, sizeof(click_ip));

  // set up UDP header
  udp->uh_sport = htons(_sport);
  udp->uh_dport = htons(_dport);
  unsigned short len = _len-14-sizeof(click_ip);
  udp->uh_ulen = htons(len);
  if (_cksum) {
    unsigned csum = ~in_cksum((unsigned char *)udp, len) & 0xFFFF;
    udp->uh_sum = csum_tcpudp_magic(_sipaddr.s_addr, _dipaddr.s_addr,
				    len, IP_PROTO_UDP, csum);
  } else
    udp->uh_sum = 0;
    
  _skb = _packet->steal_skb();
  return 0;
}

void
FastUDPSource::uninitialize()
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

  if (_limit != NO_LIMIT && _count >= _limit) return 0;

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

  if(p){
    _count++;
    if(_count == 1)
      _first = click_jiffies();
    if(_limit != NO_LIMIT && _count >= _limit)
      _last = click_jiffies();
  }

  return(p);
}

void
FastUDPSource::reset()
{
  _count = 0;
  _first = 0;
  _last = 0;
}

static String
FastUDPSource_read_count_handler(Element *e, void *)
{
  FastUDPSource *c = (FastUDPSource *)e;
  return String(c->count()) + "\n";
}

static String
FastUDPSource_read_rate_handler(Element *e, void *)
{
  FastUDPSource *c = (FastUDPSource *)e;
  int d = c->last() - c->first();
  if (d < 1) d = 1;
  int rate = c->count() * CLICK_HZ / d;
  return String(rate) + "\n";
}

static int
FastUDPSource_reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  FastUDPSource *c = (FastUDPSource *)e;
  c->reset();
  return 0;
}

void
FastUDPSource::add_handlers()
{
  add_read_handler("count", FastUDPSource_read_count_handler, 0);
  add_read_handler("rate", FastUDPSource_read_rate_handler, 0);
  add_write_handler("reset", FastUDPSource_reset_write_handler, 0);
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(FastUDPSource)

