/*
 * decip6hlim.{cc,hh} -- element decrements IP6 packet's time-to-live
 * Peilei Fan
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
#include "decip6hlim.hh"
#include "click_ip6.h"
#include "glue.hh"

DecIP6HLIM::DecIP6HLIM()
  : _drops(0)
{
  add_input();
  add_output();
}

DecIP6HLIM::~DecIP6HLIM()
{
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
