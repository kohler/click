/*
 * markipce.{cc,hh} -- element marks IP header ECN CE bit
 * Eddie Kohler
 *
 * Copyright (c) 2001 ACIRI
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/package.hh>
#include "markipce.hh"
#include <click/click_ip.h>
#include <click/confparse.hh>
#include <click/error.hh>

MarkIPCE::MarkIPCE()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

MarkIPCE::~MarkIPCE()
{
  MOD_DEC_USE_COUNT;
}

int
MarkIPCE::initialize(ErrorHandler *)
{
  _drops = 0;
  return 0;
}

inline Packet *
MarkIPCE::smaction(Packet *p)
{
  const click_ip *iph = p->ip_header();

  if (!iph || (iph->ip_tos & IP_ECNMASK) == IP_ECN_NOT_ECT) {
    p->kill();
    return 0;
  } else if ((iph->ip_tos & IP_ECNMASK) == IP_ECN_CE)
    return p;
  else {
    WritablePacket *q = p->uniqueify();
    click_ip *q_iph = q->ip_header();
  
    // incrementally update IP checksum
    // new_sum = ~(~old_sum + ~old_halfword + new_halfword)
    //         = ~(~old_sum + ~old_halfword + (old_halfword + 0x0001))
    //         = ~(~old_sum + ~old_halfword + old_halfword + 0x0001)
    //         = ~(~old_sum + ~0 + 0x0001)
    //         = ~(~old_sum + 0x0001)
    if ((q_iph->ip_tos & IP_ECNMASK) == IP_ECN_ECT2) {
      unsigned sum = (~ntohs(q_iph->ip_sum) & 0xFFFF) + 0x0001;
      q_iph->ip_sum = ~htons(sum + (sum >> 16));
    } else {
      unsigned sum = (~ntohs(q_iph->ip_sum) & 0xFFFF) + 0x0002;
      q_iph->ip_sum = ~htons(sum + (sum >> 16));
    }

    q_iph->ip_tos |= IP_ECN_CE;
    
    return q;
  }
}

void
MarkIPCE::push(int, Packet *p)
{
  if ((p = smaction(p)) != 0)
    output(0).push(p);
}

Packet *
MarkIPCE::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    p = smaction(p);
  return p;
}

String
MarkIPCE::read_handler(Element *e, void *)
{
  MarkIPCE *m = (MarkIPCE *)e;
  return String(m->_drops) + "\n";
}

void
MarkIPCE::add_handlers()
{
  add_read_handler("drops", read_handler, (void *)0);
}

EXPORT_ELEMENT(MarkIPCE)
ELEMENT_MT_SAFE(MarkIPCE)
