/*
 * copytcpseq.{cc,hh} -- copies and sets tcp sequence number
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
#include <click/click_tcp.h>
#include "copytcpseq.hh"

CopyTCPSeq::CopyTCPSeq()
  : Element(2, 2)
{
  MOD_INC_USE_COUNT;
}

CopyTCPSeq::~CopyTCPSeq()
{
  MOD_DEC_USE_COUNT;
}

int
CopyTCPSeq::configure(const Vector<String> &, ErrorHandler *)
{
  return 0;
}


int
CopyTCPSeq::initialize(ErrorHandler *)
{
  _start = false;
  return 0;
}

void
CopyTCPSeq::uninitialize()
{
}

void
CopyTCPSeq::push(int port, Packet *p)
{
  if (port == 0)
    monitor(p);
  else
    set(p);
  output(port).push(p);
}

Packet *
CopyTCPSeq::pull(int port)
{
  Packet *p = input(port).pull();
  if (p) {
    if (port == 0) {
      monitor(p);
      return p;
    }
    else {
      set(p);
      return p;
    }
  }
  return 0;
}

void
CopyTCPSeq::monitor(Packet *p)
{
  const click_tcp *tcph =
    reinterpret_cast<const click_tcp *>(p->transport_header());
  unsigned seq = ntohl(tcph->th_seq);
  if (!_start) {
    _seq = seq;
    _start = true;
  } else {
    if (SEQ_GT(seq, _seq))
      _seq = seq;
  }
}

void
CopyTCPSeq::set(Packet *p)
{
  click_tcp *tcph =
    reinterpret_cast<click_tcp *>(p->uniqueify()->transport_header());
  tcph->th_seq = htonl(_seq);
}

void
CopyTCPSeq::add_handlers()
{ 
  add_write_handler("reset", reset_write_handler, 0);
}

int
CopyTCPSeq::reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  (reinterpret_cast<CopyTCPSeq*>(e))->_start = false;
  return 0;
}

EXPORT_ELEMENT(CopyTCPSeq)

