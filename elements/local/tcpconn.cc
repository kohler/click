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
#include <click/confparse.hh>
#include <click/click_ip.h>
#include <click/click_tcp.h>
#include <click/elemfilter.hh>
#include <click/router.hh>
#include <click/error.hh>
#include "tcpdemux.hh"
#include "tcpconn.hh"

TCPConn::TCPConn()
  : Element(2, 3)
{
  MOD_INC_USE_COUNT;
}

TCPConn::~TCPConn()
{
  MOD_DEC_USE_COUNT;
}

int
TCPConn::configure(const Vector<String> &, ErrorHandler *)
{
  return 0;
}

int
TCPConn::initialize(ErrorHandler *errh)
{
  CastElementFilter filter("TCPDemux");
  Vector<Element*> tcpdemuxes;
  
  if (router()->upstream_elements(this, 0, &filter, tcpdemuxes) < 0)
    return errh->error("flow-based router context failure");
  if (tcpdemuxes.size() < 1)
    return errh->error
      ("%d upstream elements found, expecting at least 1", tcpdemuxes.size());

  for(int i=0; i<tcpdemuxes.size(); i++) {
    _tcpdemux = reinterpret_cast<TCPDemux*>(tcpdemuxes[i]->cast("TCPDemux"));
    if (_tcpdemux)
      break;
  }
  if (!_tcpdemux)
    return errh->error("no TCPDemux element found!");
  return 0;
}

void
TCPConn::uninitialize()
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
  if (port == 0)
    iput(p);
  else
    oput(p);
}

void
TCPConn::iput(Packet *p)
{
  const click_tcp *tcph = 
    reinterpret_cast<const click_tcp *>(p->transport_header());

  if (_listen) {
    if (tcph->th_flags & TH_SYN) {
      // XXX create new connection, etc... need to use as little
      // state as possible to prevent SYN attack; use RR list
      p->kill();
    }
    else
      p->kill();
  }
  else {
    // XXX implement TCB stuff for handling SYN ACK, RST and FIN packets
    // XXX verify sequence range
    output(0).push(p);
  }
}

void
TCPConn::oput(Packet *p)
{
  if (_listen) {
    p->kill();
  }
  else {
    click_ip *iph = p->uniqueify()->ip_header();
    click_tcp *tcph = 
      reinterpret_cast<click_tcp *>(p->uniqueify()->transport_header());
    unsigned int sa = _flow.saddr();
    unsigned int da = _flow.daddr();
    memmove((void *) &(iph->ip_src), (void *) &sa, 4);
    memmove((void *) &(iph->ip_dst), (void *) &da, 4);
    tcph->th_sport = _flow.sport();
    tcph->th_dport = _flow.dport();
    output(1).push(p);
  }
}
  
bool
TCPConn::connect_handler(IPFlowID f)
{ 
  assert(!_active);
  if (_tcpdemux->add_flow(f.saddr(), f.sport(), f.daddr(), f.dport(), 0)) {
    _active = true;
    _listen = false;
    _flow = f;
    _seq_nxt = random();
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

  output(2).push(q);
}

void
TCPConn::add_handlers()
{ 
  add_write_handler("ctrl", ctrl_write_handler, 0);
  add_write_handler("reset", reset_write_handler, 0);
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
  unsigned short port0, port1;
  port0 = port1 = 0;

  if(cp_va_space_parse(s, e, errh, 
	cpString, "action", &action,
	cpString, "source address", &str_addr0,
	cpInteger, "source port", &port0, 
	cpOptional,
	cpString, "destination address", &str_addr1,
	cpInteger, "destinatin port", &port1, 0) < 0)
    return -1;
  addr0 = IPAddress(str_addr0);
  addr1 = IPAddress(str_addr1);

  if (action == "connect")
    if (!(reinterpret_cast<TCPConn*>(e))->connect_handler
	   (IPFlowID(addr0,htons(port0),addr1,htons(port1))))
      return errh->error("cannot connect");
  else if (action == "listen")
    if (!(reinterpret_cast<TCPConn*>(e))->listen_handler
           (IPFlowID(addr0,htons(port0),0,0)))
      return errh->error("cannot listen");
  else
   return errh->error("action must be connect or listen\n");
  return 0;
}

EXPORT_ELEMENT(TCPConn)

