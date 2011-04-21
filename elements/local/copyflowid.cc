/*
 * copyflowid.{cc,hh} -- copies and sets tcp state
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
#include <clicknet/tcp.h>
#include "copyflowid.hh"
CLICK_DECLS

CopyFlowID::CopyFlowID()
{
}

CopyFlowID::~CopyFlowID()
{
}

int
CopyFlowID::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}


int
CopyFlowID::initialize(ErrorHandler *)
{
  _start = false;
  return 0;
}

void
CopyFlowID::push(int port, Packet *p)
{
  if (port == 0)
    monitor(p);
  else
    p = set(p);
  output(port).push(p);
}

Packet *
CopyFlowID::pull(int port)
{
  Packet *p = input(port).pull();
  if (p) {
    if (port == 0) {
      monitor(p);
      return p;
    }
    else
      return set(p);
  }
  return 0;
}

void
CopyFlowID::monitor(Packet *p)
{
  if (!_start) {
    _flow = IPFlowID(p);
    _start = true;
  }
}

Packet *
CopyFlowID::set(Packet *p)
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
    return q;
  } else
    return 0;
}

void
CopyFlowID::add_handlers()
{
  add_write_handler("reset", reset_write_handler, 0, Handler::BUTTON);
}

int
CopyFlowID::reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  (reinterpret_cast<CopyFlowID*>(e))->_start = false;
  return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CopyFlowID)
