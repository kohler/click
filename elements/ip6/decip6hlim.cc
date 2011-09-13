/*
 * decip6hlim.{cc,hh} -- element decrements IP6 packet's time-to-live
 * Peilei Fan
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
#include "decip6hlim.hh"
#include <clicknet/ip6.h>
#include <click/glue.hh>
CLICK_DECLS

DecIP6HLIM::DecIP6HLIM()
  : _drops(0)
{
}

DecIP6HLIM::~DecIP6HLIM()
{
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
  assert(p_in->has_network_header());
  const click_ip6 *ip_in = p_in->ip6_header();

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
  return String(f->drops());
}

void
DecIP6HLIM::add_handlers()
{
  add_read_handler("drops", DecIP6HLIM_read_drops);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DecIP6HLIM)
