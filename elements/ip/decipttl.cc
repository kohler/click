/*
 * decipttl.{cc,hh} -- element decrements IP packet's time-to-live
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "decipttl.hh"
#include <click/click_ip.h>
#include <click/glue.hh>

DecIPTTL::DecIPTTL()
  : _drops(0)
{
  add_input();
  add_output();
}

DecIPTTL::~DecIPTTL()
{
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
