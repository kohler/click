/*
 * tcpack.{cc,hh} -- provides TCP like acknowledgement service
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
#include "tcpbuffer.hh"
#include "tcpack.hh"
CLICK_DECLS

TCPAck::TCPAck()
  : _timer(this)
{
}

TCPAck::~TCPAck()
{
}

int
TCPAck::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _ackdelay_ms = 20;
  return Args(conf, this, errh)
      .read("DELAY", SecondsArg(3), _ackdelay_ms)
      .complete();
}


int
TCPAck::initialize(ErrorHandler *errh)
{
  ElementCastTracker filter(router(), "TCPBuffer");
  if (router()->visit_downstream(this, 0, &filter) < 0)
    return errh->error("flow-based router context failure");
  if (filter.size() < 1)
      return errh->error("need at least 1 downstream TCPBuffer");
  _tcpbuffer = reinterpret_cast<TCPBuffer *>(filter[0]->cast("TCPBuffer"));

  _synack = false;
  _needack = false;
  _timer.initialize(this);
  _timer.schedule_after_msec(_ackdelay_ms);
  return 0;
}

void
TCPAck::push(int port, Packet *p)
{
  bool forward;
  if (port == 0)
    forward = iput(p);
  else
    forward = oput(p);
  if (forward)
    output(port).push(p);
  else
    p->kill();
}

Packet *
TCPAck::pull(int port)
{
  bool forward;
  Packet *p = input(port).pull();
  if (p) {
    if (port == 0)
      forward = iput(p);
    else
      forward = oput(p);
    if (forward)
      return p;
    else {
      p->kill();
      return 0;
    }
  }
  return 0;
}

bool
TCPAck::iput(Packet *p)
{
  const click_tcp *tcph = p->tcp_header();
  if (!_synack && (tcph->th_flags&(TH_SYN|TH_ACK))==(TH_SYN|TH_ACK)) {
    // synack on input, meaning next ackn is the seqn in packet
    _ack_nxt = ntohl(tcph->th_seq)+1;
    _synack = true;
  }
  if (!_synack)
    return false;

  if (tcph->th_flags & (TH_SYN|TH_FIN|TH_RST))
    return true;

  // determine what the next ack should be
  if (TCPBuffer::seqno(p) == _ack_nxt) {
    // if the packet is what we expected, increment the expected
    // sequence number, and search for the next expected sequence
    // number from the tcp buffer.
    _ack_nxt += TCPBuffer::seqlen(p);
    bool v = _tcpbuffer->next_missing_seq_no(_ack_nxt, _ack_nxt);
    // passed seq, buffer cannot be empty
    assert(v);
  } else {
    click_chatter("seqno < ack_nxt: out of order or retransmitted packet");
  }

  _needack = true;
  if (!_timer.scheduled())
    _timer.schedule_after_msec(_ackdelay_ms);
  return true;
}

bool
TCPAck::oput(Packet *p)
{
  const click_tcp *tcph = p->tcp_header();
  if ((tcph->th_flags&(TH_SYN|TH_ACK)) == (TH_SYN|TH_ACK)) {
    // synack on output, meaning next ackn is the ackn in packet
    _ack_nxt = ntohl(tcph->th_ack);
    _synack = true;
  }
  if (!_synack)
    return false;
  _needack = false;
  // XXX BUGGY, BUGGY CODE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  click_tcp *tcph_new = p->uniqueify()->tcp_header();
  tcph_new->th_ack = htonl(_ack_nxt);
  return true;
}

void
TCPAck::run_timer(Timer *)
{
  if (_needack) {
    send_ack();
    _needack = false;
  }
}

void
TCPAck::send_ack()
{
  struct click_ip *ip;
  struct click_tcp *tcp;
  WritablePacket *q = Packet::make(sizeof(*ip) + sizeof(*tcp));
  if (q == 0) {
    click_chatter("TCPAck: cannot make packet");
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

  tcp->th_ack = htonl(_ack_nxt);
  tcp->th_off = 5;
  tcp->th_flags = TH_ACK;
  tcp->th_win = htons(32120); // when and where should this be set?
  tcp->th_sum = htons(0);
  tcp->th_urp = htons(0);

  q->set_ip_header(ip, ip->ip_hl << 2);
  output(2).push(q);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPAck)
