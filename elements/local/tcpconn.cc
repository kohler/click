/*
 * tcpconn.{cc,hh} -- create or listen for tcp connections
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
#include <click/routervisitor.hh>
#include <click/router.hh>
#include <click/error.hh>
#include "tcpdemux.hh"
#include "tcpbuffer.hh"
#include "tcpconn.hh"
CLICK_DECLS

TCPConn::TCPConn()
  : _active(false)
{
}

TCPConn::~TCPConn()
{
}

int
TCPConn::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

int
TCPConn::initialize(ErrorHandler *errh)
{
  ElementCastTracker filter(router(), "TCPDemux");
  if (router()->visit_upstream(this, 0, &filter) < 0)
    return errh->error("flow-based router context failure");
  if (filter.size() < 1)
      return errh->error("need at least 1 upstream TCPDemux");
  _tcpdemux = reinterpret_cast<TCPDemux *>(filter[0]->cast("TCPDemux"));
  return 0;
}

void
TCPConn::cleanup(CleanupStage)
{
  reset();
}

void
TCPConn::reset()
{
  if (_active) {
    _tcpdemux->remove_flow
      (_flow.saddr(), _flow.sport(), _flow.daddr(), _flow.dport());
    _active = false;
  }
}

void
TCPConn::push(int port, Packet *p)
{
  assert(port == 0);
  if (iput(p))
    output(0).push(p);
  else
    p->kill();
}

Packet*
TCPConn::pull(int port)
{
  if (!_active || _listen || !_established)
    return 0;

  assert(port == 1);
  Packet *p = input(1).pull();
  if (p)
    return oput(p);
  return 0;
}

bool
TCPConn::iput(Packet *p)
{
  const click_tcp *tcph = p->tcp_header();

  if (_listen) {
    if (tcph->th_flags & TH_SYN) {
      // XXX create new connection, etc... need to use as little
      // state as possible to prevent SYN attack; use RR list
      return false;
    }
    else
      return false;
  }
  else {
    // XXX verify sequence range
    if ((tcph->th_flags&(TH_SYN|TH_ACK)) == (TH_SYN|TH_ACK)) {
      _established = true;
      _seq_nxt++;
    }
    // XXX implement TCB stuff for handling SYN ACK, RST and FIN packets
    return true;
  }
}

Packet *
TCPConn::oput(Packet *p)
{
  if (WritablePacket *q = p->uniqueify()) {
    click_ip *iph = q->ip_header();
    click_tcp *tcph = q->tcp_header();
    unsigned int sa = _flow.saddr();
    unsigned int da = _flow.daddr();
    memmove((void *) &(iph->ip_src), (void *) &sa, 4);
    memmove((void *) &(iph->ip_dst), (void *) &da, 4);
    tcph->th_sport = _flow.sport();
    tcph->th_dport = _flow.dport();
    tcph->th_seq = ntohl(_seq_nxt);
    _seq_nxt += TCPBuffer::seqlen(p);
    return q;
  } else
    return 0;
}

bool
TCPConn::connect_handler(IPFlowID f)
{
  assert(!_active);
  if (_tcpdemux->add_flow(f.saddr(), f.sport(), f.daddr(), f.dport(), 0)) {
    _active = true;
    _listen = false;
    _established = false;
    _flow = f;
    _seq_nxt = click_random();
    send_syn();
    return true;
  }
  else
    return false;
}

bool
TCPConn::listen_handler(IPFlowID f)
{
  assert(!_active);
  if (_tcpdemux->add_flow(f.saddr(), f.sport(), f.daddr(), f.dport(), 0)) {
    _active = true;
    _listen = true;
    _established = false;
    _flow = f;
    return true;
  }
  else
    return false;
}

void
TCPConn::send_syn()
{
  struct click_ip *ip;
  struct click_tcp *tcp;
  WritablePacket *q = Packet::make(sizeof(*ip) + sizeof(*tcp));
  if (q == 0) {
    click_chatter("TCPConn: cannot make packet");
    return;
  }
  memset(q->data(), '\0', q->length());
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
  unsigned int sa = _flow.saddr();
  unsigned int da = _flow.daddr();
  memmove((void *) &(ip->ip_src), (void *) &sa, 4);
  memmove((void *) &(ip->ip_dst), (void *) &da, 4);

  tcp->th_sport = _flow.sport();
  tcp->th_dport = _flow.dport();
  tcp->th_seq = htonl(_seq_nxt);
  tcp->th_ack = 0;
  tcp->th_off = 5;
  tcp->th_flags = TH_SYN;
  tcp->th_win = htons(32120); // when and where should this be set?
  tcp->th_sum = htons(0);
  tcp->th_urp = htons(0);

  q->set_ip_header(ip, ip->ip_hl << 2);
  output(2).push(q);
}

void
TCPConn::add_handlers()
{
  add_write_handler("ctrl", ctrl_write_handler, 0);
  add_write_handler("reset", reset_write_handler, 0, Handler::BUTTON);
}

int
TCPConn::reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  (reinterpret_cast<TCPConn*>(e))->reset();
  return 0;
}

int
TCPConn::ctrl_write_handler
(const String &s, Element *e, void *, ErrorHandler *errh)
{
  if ((reinterpret_cast<TCPConn*>(e))->_active)
    return errh->error("TCPConn already active");

  String action;
  String str_addr0, str_addr1;
  IPAddress addr0, addr1;
  uint16_t port0 = 0, port1 = 0;

  if (Args(e, errh).push_back_words(s)
      .read_mp("ACTION", action)
      .read_mp("SRC", str_addr0)
      .read_mp("SPORT", IPPortArg(IP_PROTO_TCP), port0)
      .read_p("DST", str_addr1)
      .read_p("DPORT", IPPortArg(IP_PROTO_TCP), port1)
      .complete() < 0)
    return -1;
  addr0 = IPAddress(str_addr0);
  addr1 = IPAddress(str_addr1);

  if (action == "connect") {
    if (!(reinterpret_cast<TCPConn*>(e))->connect_handler
	   (IPFlowID(addr0,htons(port0),addr1,htons(port1))))
      return errh->error("cannot connect");
  } else if (action == "listen") {
    if (!(reinterpret_cast<TCPConn*>(e))->listen_handler
           (IPFlowID(addr0,htons(port0),0,0)))
      return errh->error("cannot listen");
  } else
   return errh->error("action must be connect or listen\n");
  return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPConn)
