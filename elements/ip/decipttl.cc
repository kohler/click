/*
 * decipttl.{cc,hh} -- element decrements IP packet's time-to-live
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "decipttl.hh"
#include "click_ip.h"
#include "glue.hh"

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

void
DecIPTTL::processing_vector(Vector<int> &in_v, int in_offset,
			      Vector<int> &out_v, int out_offset) const
{
  in_v[in_offset+0] = out_v[out_offset+0] = AGNOSTIC;
  if (noutputs() == 2)
    out_v[out_offset+1] = PUSH;
}

DecIPTTL *
DecIPTTL::clone() const
{
  return new DecIPTTL;
}

Packet *
DecIPTTL::simple_action(Packet *p)
{
  click_ip *ip = p->ip_header();
  assert(ip);
  
  if (ip->ip_ttl <= 1) {
    _drops++;
    if (noutputs() == 2)
      output(1).push(p);
    else
      p->kill();
    return 0;
    
  } else {
    p = p->uniqueify();
    ip = p->ip_header();
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

#if 0
inline Packet *
DecIPTTL::smaction(Packet *p)
{
  click_ip *ip = p->ip_header();
  assert(ip);
  
  if (ip->ip_ttl <= 1) {
    _drops++;
    if (noutputs() == 2)
      output(1).push(p);
    else
      p->kill();
    return 0;
    
  } else {
    p = p->uniqueify();
    ip = p->ip_header();
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

void
DecIPTTL::push(int, Packet *p)
{
  if ((p = smaction(p)) != 0)
    output(0).push(p);
}

Packet *
DecIPTTL::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    p = smaction(p);
  return p;
}
#endif

static String
DecIPTTL_read_drops(Element *xf, void *)
{
  DecIPTTL *f = (DecIPTTL *)xf;
  return String(f->drops()) + "\n";
}

void
DecIPTTL::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("drops", DecIPTTL_read_drops, 0);
}

EXPORT_ELEMENT(DecIPTTL)
