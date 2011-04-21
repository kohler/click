/*
 * ToyTCP.{cc,hh} -- toy TCP implementation
 * Robert Morris
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
#include "toytcp.hh"
#include <clicknet/tcp.h>
#include <clicknet/ip.h>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

ToyTCP::ToyTCP()
  : _timer(this)
{

  _dport = 0;

  _ingood = 0;
  _inbad = 0;
  _out = 0;

  restart();
}

void
ToyTCP::restart()
{
  Timestamp tv = Timestamp::now();

  _iss = tv.sec() & 0x0fffffff;
  _irs = 0;
  _snd_nxt = _iss;
  _sport = htons(1024 + (tv.usec() % 60000));
  _state = 0;
  _grow = 0;
  _wc = 0;
  _reset = 0;
}

ToyTCP::~ToyTCP()
{
}

int
ToyTCP::configure(Vector<String> &conf, ErrorHandler *errh)
{
  uint16_t dport;
  int ret;

  ret = Args(conf, this, errh)
      .read_mp("DPORT", IPPortArg(IP_PROTO_TCP), dport)
      .complete();
  if(ret < 0)
    return(ret);

  _dport = htons(dport);

  return(0);
}

int
ToyTCP::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_msec(1000);
  return 0;
}

void
ToyTCP::run_timer(Timer *)
{
  if(_reset)
    restart();
  tcp_output(0);
  _timer.schedule_after_msec(1000);
  click_chatter("ToyTCP: %d good in, %d bad in, %d out",
                _ingood, _inbad, _out);
}

void
ToyTCP::tcp_input(Packet *p)
{
  click_tcp *th = (click_tcp *) p->data();
  unsigned seq, ack;

  if(p->length() < sizeof(*th))
    return;
  if(th->th_sport != _dport || th->th_dport != _sport)
    return;
  seq = ntohl(th->th_seq);
  ack = ntohl(th->th_ack);

  if((th->th_flags & (TH_ACK|TH_RST)) == TH_ACK &&
     ack == _iss + 1 &&
     _state == 0){
    _snd_nxt = _iss + 1;
    _irs = seq;
    _rcv_nxt = _irs + 1;
    _state = 1;
    click_chatter("ToyTCP connected");
  }

  if(th->th_flags & TH_RST){
    click_chatter("ToyTCP: RST from port %d, in %d, out %d",
                  ntohs(th->th_sport),
                  _ingood, _out);
    _inbad++;
    _reset = 1;
  } else {
    _ingood++;
  }
}

Packet *
ToyTCP::simple_action(Packet *p)
{
  if(_reset){
    p->kill();
  } else {
    tcp_input(p);
    tcp_output(p);
    if(_grow++ > 4){
      tcp_output(0);
      _grow = 0;
    }
  }
  return(0);
}

// Send a suitable TCP packet.
// xp is a candidate packet buffer, to be re-used or freed.
void
ToyTCP::tcp_output(Packet *xp)
{
  int paylen = _state ? 1 : 0;
  unsigned int plen = sizeof(click_tcp) + paylen;
  unsigned int headroom = 34;
  WritablePacket *p = 0;

  if(xp == 0 ||
     xp->shared() ||
     xp->headroom() < headroom ||
     xp->length() + xp->tailroom() < plen){
    if(xp){
      click_chatter("could not re-use %d %d %d",
                    xp->headroom(), xp->length(), xp->tailroom());
      xp->kill();
    }
    p = Packet::make(headroom, (const unsigned char *)0, plen, 0);
  } else {
    p = xp->uniqueify();
    if (p->length() > plen)
	p->take(p->length() - plen);
    else if (p->length() < plen)
	if (!(p = p->put(plen - p->length())))
	    return;
  }

  click_tcp *th = (click_tcp *) p->data();

  memset(th, '\0', sizeof(*th));

  th->th_sport = _sport;
  th->th_dport = _dport;
  if(_state){
    th->th_seq = htonl(_snd_nxt + 1 + (_out & 0xfff));
  } else {
    th->th_seq = htonl(_snd_nxt);
  }
  th->th_off = sizeof(click_tcp) >> 2;
  if(_state == 0){
    th->th_flags = TH_SYN;
  } else {
    th->th_flags = TH_ACK;
    th->th_ack = htonl(_rcv_nxt);
  }

  if(_wc++ > 2){
    _wc = 0;
    th->th_win = htons(30*1024);
  } else {
    th->th_win = htons(60*1024);
  }

  output(0).push(p);

  _out++;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ToyTCP)
