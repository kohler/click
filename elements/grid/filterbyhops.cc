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

#include <stddef.h>
#include <click/config.h>
#include "filterbyhops.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/click_ether.h>
#include <click/glue.hh>
#include "grid.hh"
#include <math.h>

FilterByGridHops::FilterByGridHops() : Element(1, 2)
{
  MOD_INC_USE_COUNT;
}

FilterByGridHops::~FilterByGridHops()
{
  MOD_DEC_USE_COUNT;
}

int
FilterByGridHops::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int mh;
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "max hops", &mh,
			0);
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


ELEMENT_REQUIRES(userlevel GridLocationInfo)
EXPORT_ELEMENT(FilterByGridHops)
