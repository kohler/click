/*
 * tcpbuffer.{cc,hh} -- provides a TCP buffer
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
#include "tcpbuffer.hh"

TCPBuffer::TCPBuffer()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

TCPBuffer::~TCPBuffer()
{
  MOD_DEC_USE_COUNT;
}

int
TCPBuffer::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _skip = false;
  return cp_va_parse(conf, this, errh, 
                     cpOptional, cpBool, "skip missing packets", &_skip, 0);
}


int
TCPBuffer::initialize(ErrorHandler *)
{
  _chain = 0;
  _initial_seq = 0;
  _first_seq = 0;
  return 0;
}

void
TCPBuffer::uninitialize()
{
  TCPBufferElt *elt = _chain;
  while (elt) {
    TCPBufferElt *t = elt;
    elt = elt->next();
    Packet *p = t->kill_elt();
    p->kill();
  }
  assert(_chain == 0);
}

void
TCPBuffer::push(int, Packet *p)
{
  const click_tcp *tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
  if (_initial_seq == 0)
    _initial_seq = ntohl(tcph->th_seq);
  else if (_first_seq > 0 && ntohl(tcph->th_seq) < _first_seq) {
    // click_chatter("retrans packet, rejected");
    p->kill();
    return;
  }
  new TCPBufferElt(&_chain, p);

#if 0
  click_chatter("seq0 %u, seq %u", _initial_seq, _first_seq);
  TCPBufferElt *elt = _chain;
  while(elt) {
    Packet *pp = elt->packet();
    const click_tcp *tcph = reinterpret_cast<const click_tcp *>(pp->transport_header());
    click_chatter("elt %p (%p): %u", elt, pp, ntohl(tcph->th_seq));
    elt = elt->next();
  }
#endif
}

Packet *
TCPBuffer::pull(int)
{
  if (_chain) {
    Packet *p = _chain->packet();
    const click_ip *iph = p->ip_header();
    const click_tcp *tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
    if (_first_seq == 0 || _skip || ntohl(tcph->th_seq)==_first_seq) {
      _chain->kill_elt();
      unsigned seqlen = (ntohs(iph->ip_len)-(iph->ip_hl<<2)-(tcph->th_off<<2)); 
      if ((tcph->th_flags&TH_SYN) || (tcph->th_flags&TH_FIN)) seqlen++;
      _first_seq = ntohl(tcph->th_seq) + seqlen;
#if 0
      click_chatter("new seq0 %u, seq %u, %u+%d",
	            _initial_seq, _first_seq, ntohl(tcph->th_seq), seqlen);
#endif
      return p;
    }
  }
  return 0;
}

EXPORT_ELEMENT(TCPBuffer)

