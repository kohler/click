/*
 * decip6hlim.{cc,hh} -- element decrements IP6 packet's time-to-live
 * Peilei Fan
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

#include <click/config.h>
#include <click/package.hh>
#include "decip6hlim.hh"
#include <click/click_ip6.h>
#include <click/glue.hh>

DecIP6HLIM::DecIP6HLIM()
  : _drops(0)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

DecIP6HLIM::~DecIP6HLIM()
{
  MOD_DEC_USE_COUNT;
}

void
DecIP6HLIM::notify_noutputs(int n)
{
  // allow 2 outputs -- then packet is pushed onto 2d output instead of
  // dropped
  set_noutputs(n < 2 ? 1 : 2);
}

DecIP6HLIM *
DecIP6HLIM::clone() const
{
  return new DecIP6HLIM;
}

void
DecIP6HLIM::drop_it(Packet *p)
{
  _drops++;
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
}

inline Packet *
DecIP6HLIM::simple_action(Packet *p_in)
{
  
  const click_ip6 *ip_in = p_in->ip6_header();
  assert(ip_in);
  
  if (ip_in->ip6_hlim <= 1) {
    drop_it(p_in);
    return 0;
  } else {
     WritablePacket *p = p_in->uniqueify();
     click_ip6 *ip = p->ip6_header();
     ip->ip6_hlim--;
    return p;
  }
}

static String
DecIP6HLIM_read_drops(Element *xf, void *)
{
  DecIP6HLIM *f = (DecIP6HLIM *)xf;
  return String(f->drops()) + "\n";
}

void
DecIP6HLIM::add_handlers()
{
  add_read_handler("drops", DecIP6HLIM_read_drops, 0);
}

EXPORT_ELEMENT(DecIP6HLIM)
