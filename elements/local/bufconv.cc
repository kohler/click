/*
 * bufconv.{cc,hh} -- converts buffers from/to tcp packets
 * Benjie Chen
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/timer.hh>
#include <click/error.hh>
#include "bufconv.hh"
CLICK_DECLS

BufferConverter::BufferConverter()
  : _timer(this)
{
}

BufferConverter::~BufferConverter()
{
}

int
BufferConverter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("MTU", _mtu).complete();
}

int
BufferConverter::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  return 0;
}

void
BufferConverter::push(int, Packet *p)
{
  // XXX do path mtu discovery
  p->kill();
}

void
BufferConverter::run_timer(Timer *)
{
  _timer.schedule_after_msec(packet_tx_delay);
}

void
BufferConverter::oput(const String &s)
{
  int dlen = _obuf.length() + s.length();
  if (dlen > _mtu)
    dlen = _mtu;

  struct click_ip *ip;
  struct click_tcp *tcp;
  WritablePacket *q = Packet::make(sizeof(*ip) + sizeof(*tcp) + dlen);
  if (q == 0) {
    click_chatter("BufferConverter: cannot make packet");
    return;
  }
  memset(q->data(), '\0', sizeof(*ip)+sizeof(*tcp));
  ip = (struct click_ip *) q->data();
  tcp = (struct click_tcp *) (ip + 1);
  ip->ip_v = 4;
  ip->ip_hl = 5;
  ip->ip_tos = 0x10;
  ip->ip_len = htons(q->length());
  ip->ip_id = htons(0); // what is this used for exactly?
  ip->ip_off = htons(IP_DF);
  ip->ip_ttl = 255;
  ip->ip_p = IP_PROTO_TCP;
  ip->ip_sum = 0;

  tcp->th_off = 5;
  tcp->th_flags = TH_PUSH; // how should we set the PUSH bit?
  tcp->th_win = htons(32120); // when and where should this be set?
  tcp->th_sum = htons(0);
  tcp->th_urp = htons(0);

  unsigned char *data = q->data() + sizeof(*ip) + sizeof(*tcp);
  if (_obuf.length() > _mtu) {
    memmove(data, _obuf.c_str(), _mtu);
    _obuf = _obuf.substring(_mtu, _obuf.length()-_mtu);
    dlen -= _mtu;
    data += _mtu;
  }
  else if (_obuf.length() > 0) {
    memmove(data, _obuf.c_str(), _obuf.length());
    dlen -= _obuf.length();
    data += _obuf.length();
    _obuf = "";
  }
  String ss = s;
  memmove(data, ss.data(), dlen);
  if (ss.length() > dlen)
    _obuf += ss.substring(dlen, ss.length()-dlen);

  q->set_ip_header(ip, ip->ip_hl << 2);
  output(0).push(q);
}

String
BufferConverter::iput()
{
  Packet *p;
  while((p = input(0).pull())) {
    const click_ip *iph = p->ip_header();
    const click_tcp *tcph = p->tcp_header();
    unsigned dlen = ntohs(iph->ip_len)-(iph->ip_hl<<2)-(tcph->th_off<<2);
    if (dlen == 0) {
      p->kill();
      continue;
    }
    unsigned off = tcph->th_off << 2;
    const char *data = ((const char*)tcph) + off;
    String s(data, dlen);
    p->kill();
    return s;
  }
  return "";
}

void
BufferConverter::add_handlers()
{
  add_write_handler("data", data_write_handler, 0);
  add_read_handler("data", data_read_handler, 0);
}

int
BufferConverter::data_write_handler
(const String &s, Element *e, void *, ErrorHandler *)
{
  reinterpret_cast<BufferConverter*>(e)->oput(s);
  return 0;
}

String
BufferConverter::data_read_handler(Element *e, void *)
{
  return reinterpret_cast<BufferConverter*>(e)->iput();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BufferConverter)
