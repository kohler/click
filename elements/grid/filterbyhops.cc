/*
 * filterbyhops.{cc,hh} -- Grid packet filtering element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include <stddef.h>
#include "filterbyhops.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <clicknet/ether.h>
#include <click/glue.hh>
#include "grid.hh"
//#include <math.h>
CLICK_DECLS

FilterByGridHops::FilterByGridHops()
{
}

FilterByGridHops::~FilterByGridHops()
{
}

int
FilterByGridHops::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int mh;
  int res = Args(conf, this, errh).read_mp("HOPS", mh).complete();
  if (res < 0)
    return res;


  if (mh < 0)
    return errh->error("max_hops must be positive");
  _max_hops = mh;
  return res;
}

int
FilterByGridHops::initialize(ErrorHandler *)
{
  return 0;
}

void
FilterByGridHops::push(int, Packet *p)
{
  assert(p);
  grid_nbr_encap *nb = (grid_nbr_encap *) (p->data() + sizeof(click_ether) + sizeof(grid_hdr));

  if (ntohl(nb->hops_travelled) < _max_hops)
    output(0).push(p);
  else // ``too many hops''
    output(1).push(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FilterByGridHops)
