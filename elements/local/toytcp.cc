/*
 * ToyTCP.{cc,hh} -- toy TCP implementation
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "toytcp.hh"
#include "click_tcp.h"
#include "click_ip.h"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

ToyTCP::ToyTCP()
  : _timer(this)
{
  struct timeval tv;

  add_input();
  add_output();

  click_gettimeofday(&tv);

  _sport = htons(1024 + (tv.tv_usec % 60000));
  _dport = 0;
  _state = 0;
  _irs = 0;
  _iss = tv.tv_sec & 0x0fffffff;
  _snd_nxt = _iss;
  _grow = 0;
  _wc = 0;
  _reset = 0;

  _ingood = 0;
  _inbad = 0;
  _out = 0;
}

ToyTCP::~ToyTCP()
{
}

ToyTCP *
ToyTCP::clone() const
{
  return new ToyTCP;
}

int
ToyTCP::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned dport;
  int ret;

  ret = cp_va_parse(conf, this, errh,
                    cpUnsigned, "destination port", &dport,
                    0);
  if(ret < 0)
    return(ret);

  _dport = htons(dport);

  return(0);
}

int
ToyTCP::initialize(ErrorHandler *)
{
  _timer.attach(this);
  _timer.schedule_after_ms(1000);
  return 0;
}

void
ToyTCP::run_scheduled()
{
  if(_reset == false){
    tcp_output(0);
    _timer.schedule_after_ms(1000);
    click_chatter("ToyTCP: %d good in, %d bad in, %d out",
                  _ingood, _inbad, _out);
  }
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

  if((th->th_flags & TH_ACK) &&
     ack == _iss + 1 &&
     _state == 0){
    _snd_nxt = _iss + 1;
    _irs = seq;
    _rcv_nxt = _irs + 1;
    _state = 1;
    click_chatter("ToyTCP connected");
  }

  if(th->th_flags & TH_RST){
    if(_reset == false)
      click_chatter("ToyTCP: RST, in %d, out %d",
                    _ingood, _out);
    _reset = true;
    _inbad++;
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
    p = Packet::make(headroom, (const unsigned char *)0,
                     plen,
                     Packet::default_tailroom(plen));
  } else {
    p = xp->uniqueify();
    if(p->length() < plen)
      p = p->put(plen - p->length());
    else if(p->length() > plen)
      p->take(p->length() - plen);
  }

  click_tcp *th = (click_tcp *) p->data();

  memset(th, '\0', sizeof(*th));

  th->th_sport = _sport;
  th->th_dport = _dport;
  if(_state){
    th->th_seq = htonl(_snd_nxt + (_out & 0xfff));
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

EXPORT_ELEMENT(ToyTCP)
