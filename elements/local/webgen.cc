/*
 * WebGen.{cc,hh} -- toy TCP implementation
 * Robert Morris
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
#include "webgen.hh"
#include <click/click_tcp.h>
#include <click/click_ip.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

WebGen::WebGen()
  : _timer(this)
{
  MOD_INC_USE_COUNT;

  add_input();
  add_output();

  _ncbs = 3;
  int i;
  for(i = 0; i < _ncbs; i++){
    _cbs[i] = new CB;
    _cbs[i]->_reset = 1;
  }
}

WebGen::~WebGen()
{
  MOD_DEC_USE_COUNT;
  for (int i = 0; i < _ncbs; i++) {
    delete _cbs[i];
    _cbs[i] = 0;
  }
}

int
WebGen::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int ret;

  ret = cp_va_parse(conf, this, errh,
                    cpIPAddressOrPrefix, "IP address/len", &_src_prefix, &_mask,
                    cpIPAddress, "IP address/len", &_dst,
                    0);

  return(ret);
}

IPAddress
WebGen::pick_src()
{
  uint32_t x;
  uint32_t mask = (uint32_t) _mask;

  x = (random() & ~mask) | ((uint32_t)_src_prefix & mask);
  
  return(IPAddress(x));
}

WebGen::CB::CB()
{
}

void
WebGen::CB::reset(IPAddress src)
{
  _src = src;
  _dport = htons(79); // XXX 80
  _iss = (random() & 0x0fffffff);
  _irs = 0;
  _snd_nxt = _iss;
  _snd_una = _iss;
  _sport = htons(1024 + (random() % 60000));
  _do_send = 0;
  _connected = 0;
  _got_fin = 0;
  _closed = 0;
  _reset = 0;
  _resends = 0;
}

WebGen *
WebGen::clone() const
{
  return new WebGen;
}

int
WebGen::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(1000);
  return 0;
}

void
WebGen::run_scheduled()
{
  int i;

  for(i = 0; i < _ncbs; i++){
    CB *cb = _cbs[i];
    if(cb->_reset || cb->_closed || cb->_resends > 5)
      cb->reset(pick_src());
    tcp_output(cb, 0);
    cb->_resends += 1;
  }
  _timer.schedule_after_ms(1000);
}

WebGen::CB *
WebGen::find_cb(unsigned src, unsigned short sport, unsigned short dport)
{
  int i;

  for(i = 0; i < _ncbs; i++){
    if(sport == _cbs[i]->_sport &&
       dport == _cbs[i]->_dport &&
       src == (uint32_t) _cbs[i]->_src){
      return(_cbs[i]);
    }
  }
  return(0);
}

void
WebGen::tcp_input(Packet *p)
{
  unsigned seq, ack;
  unsigned plen = p->length();

  if(plen < sizeof(click_ip) + sizeof(click_tcp))
    return;

  click_ip *ip = (click_ip *) p->data();
  unsigned hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip) || hlen > plen){
    p->kill();
    return;
  }

  click_tcp *th = (click_tcp *) (((char *)ip) + hlen);
  unsigned off = th->th_off << 2;
  int dlen = plen - hlen - off;

  CB *cb = find_cb(ip->ip_dst.s_addr, th->th_dport, th->th_sport);
  if(cb == 0 || cb->_reset){
    p->kill();
    return;
  }

  seq = ntohl(th->th_seq);
  ack = ntohl(th->th_ack);

  if((th->th_flags & (TH_ACK|TH_RST)) == TH_ACK &&
     ack == cb->_iss + 1 &&
     cb->_connected == 0){
    cb->_snd_nxt = cb->_iss + 1;
    cb->_snd_una = cb->_snd_nxt;
    cb->_irs = seq;
    cb->_rcv_nxt = cb->_irs + 1;
    cb->_connected = 1;
    cb->_do_send = 1;
    click_chatter("WebGen connected %d %d",
                  ntohs(cb->_sport),
                  ntohs(cb->_dport));
  } else if(dlen > 0){
    cb->_do_send = 1;
    if(seq + dlen > cb->_rcv_nxt)
      cb->_rcv_nxt = seq + dlen;
  }    

  if(th->th_flags & TH_ACK){
    if(ack > cb->_snd_una){
      cb->_snd_una = ack;
    }
    if(cb->_got_fin && ack > cb->_fin_seq){
      // Our FIN has been ACKed.
      cb->_closed = 1;
    }
  }

  if((th->th_flags & TH_FIN) &&
     seq + dlen == cb->_rcv_nxt &&
     cb->_got_fin == 0){
    cb->_got_fin = 1;
    cb->_fin_seq = cb->_snd_nxt;
    cb->_rcv_nxt += 1;
    cb->_do_send = 1;
  }

  if(th->th_flags & TH_RST){
    click_chatter("WebGen: RST %d %d",
                  ntohs(th->th_sport),
                  ntohs(th->th_dport));
    cb->_reset = 1;
  }

  if(cb->_reset){
    p->kill();
  } else {
    tcp_output(cb, p);
  }

  if(cb->_reset || cb->_closed){
    cb->reset(pick_src());
    tcp_output(cb, 0);
  }
}

Packet *
WebGen::simple_action(Packet *p)
{
  tcp_input(p);
  return(0);
}

// Send a suitable TCP packet.
// xp is a candidate packet buffer, to be re-used or freed.
void
WebGen::tcp_output(CB *cb, Packet *xp)
{
  int paylen;
  unsigned int plen;
  unsigned int headroom = 34;
  unsigned int seq;
  char itmp[9];
  WritablePacket *p = 0;

#define STR "GET /\n"

  if(cb->_connected && cb->_snd_una - cb->_iss - 1 < strlen(STR)){
    paylen = strlen(STR);
    seq = cb->_iss + 1;
    cb->_snd_nxt = seq + paylen;
  } else {
    paylen = 0;
    seq = cb->_snd_nxt;
  }
  plen = sizeof(click_ip) + sizeof(click_tcp) + paylen;

  if(cb->_connected == 1 && cb->_do_send == 0 && paylen == 0){
    if(xp)
      xp->kill();
    return;
  }
  cb->_do_send = 0;

  if(xp == 0 ||
     xp->shared() ||
     xp->headroom() < headroom ||
     xp->length() + xp->tailroom() < plen){
    if(xp){
      xp->kill();
    }
    p = Packet::make(headroom, (const unsigned char *)0, plen, 0);
  } else {
    p = xp->uniqueify();
    if(p->length() < plen)
      p = p->put(plen - p->length());
    else if(p->length() > plen)
      p->take(p->length() - plen);
  }

  click_ip *ip = (click_ip *) p->data();
  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id.read_and_add(1));
  ip->ip_p = 6;
  ip->ip_src = cb->_src;
  ip->ip_dst = _dst;
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 250;
  p->set_dst_ip_anno(IPAddress(ip->ip_dst));
  p->set_ip_header(ip, sizeof(click_ip));

  click_tcp *th = (click_tcp *) (ip + 1);

  memset(th, '\0', sizeof(*th));

  if(paylen > 0){
    memcpy(th + 1, STR, paylen);
  }

  th->th_sport = cb->_sport;
  th->th_dport = cb->_dport;
  th->th_seq = htonl(seq);
  th->th_off = sizeof(click_tcp) >> 2;
  if(cb->_connected == 0){
    th->th_flags = TH_SYN;
  } else {
    th->th_flags = TH_ACK;
    if(paylen)
      th->th_flags |= TH_PUSH;
    if(cb->_got_fin)
      th->th_flags |= TH_FIN;
    th->th_ack = htonl(cb->_rcv_nxt);
  }

  th->th_win = htons(60*1024);

  memcpy(itmp, ip, 9);
  memset(ip, '\0', 9);
  ip->ip_sum = 0;
  ip->ip_len = htons(plen - 20);

  th->th_sum = 0;
  th->th_sum = in_cksum((unsigned char *)ip, plen);

  memcpy(ip, itmp, 9);
  ip->ip_len = htons(plen);

  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));

  output(0).push(p);
}

EXPORT_ELEMENT(WebGen)
