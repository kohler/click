/*
 * decipttl.{cc,hh} -- element decrements IP packet's time-to-live
 * Eddie Kohler, Robert Morris
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
#include "decipttl.hh"
#include <click/click_ip.h>
#include <click/glue.hh>

DecIPTTL::DecIPTTL()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _drops = 0;
}

DecIPTTL::~DecIPTTL()
{
  MOD_DEC_USE_COUNT;
}

void
DecIPTTL::notify_noutputs(int n)
{
  // allow 2 outputs -- then packet is pushed onto 2d output instead of
  // dropped
  set_noutputs(n < 2 ? 1 : 2);
}

DecIPTTL *
DecIPTTL::clone() const
{
  return new DecIPTTL;
}

void
DecIPTTL::drop_it(Packet *p)
{
  _drops++;
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
}

Packet *
DecIPTTL::simple_action(Packet *p_in)
{
  const click_ip *ip_in = p_in->ip_header();
  assert(ip_in);
  
  if (ip_in->ip_ttl <= 1) {
    drop_it(p_in);
    return 0;
  } else {
    WritablePacket *p = p_in->uniqueify();
    click_ip *ip = p->ip_header();
    ip->ip_ttl--;
    
    // 19.Aug.1999 - incrementally update IP checksum as suggested by SOSP
    // reviewers, according to RFC1141, as updated by RFC1624.
    // new_sum = ~(~old_sum + ~old_halfword + new_halfword)
    //         = ~(~old_sum + ~old_halfword + (old_halfword - 0x0100))
    //         = ~(~old_sum + ~old_halfword + old_halfword + ~0x0100)
    //         = ~(~old_sum + ~0 + ~0x0100)
    //         = ~(~old_sum + 0xFEFF)
    unsigned long sum = (~ntohs(ip->ip_sum) & 0xFFFF) + 0xFEFF;
    ip->ip_sum = ~htons(sum + (sum >> 16));
    
    return p;
  }
}

static String
DecIPTTL_read_drops(Element *xf, void *)
{
  DecIPTTL *f = (DecIPTTL *)xf;
  return String(f->drops()) + "\n";
}

void
DecIPTTL::add_handlers()
{
  add_read_handler("drops", DecIPTTL_read_drops, 0);
}

EXPORT_ELEMENT(DecIPTTL)
ELEMENT_MT_SAFE(DecIPTTL)
