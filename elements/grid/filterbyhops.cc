/*
 * filterbyhops.{cc,hh} -- Grid packet filtering element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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

#include <stddef.h>
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "filterbyhops.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"
#include "click_ether.h"
#include "glue.hh"
#include "grid.hh"
#include <math.h>

FilterByGridHops::FilterByGridHops() : Element(1, 2)
{
}

FilterByGridHops::~FilterByGridHops()
{
}

int
FilterByGridHops::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int mh;
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "max hops", &mh,
			0);

  
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


ELEMENT_REQUIRES(userlevel LocationInfo)
EXPORT_ELEMENT(FilterByGridHops)
