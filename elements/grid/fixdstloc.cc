/*
 * fixdstloc.{cc,hh} -- element sets Grid destination location.
 * Douglas S. J. De Couto
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
#include "fixdstloc.hh"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "grid.hh"
#include "router.hh"
#include "click_ether.h"

FixDstLoc::FixDstLoc() : Element(1, 1), _loctab(0)
{
}

FixDstLoc::~FixDstLoc()
{
}

FixDstLoc *
FixDstLoc::clone() const
{
  return new FixDstLoc;
}

int
FixDstLoc::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
                        cpElement, "LocationTable element", &_loctab,
			0);
  return res;
}

int
FixDstLoc::initialize(ErrorHandler *errh)
{
  if(_locinfo && _locinfo->cast("LocationTable") == 0){
    errh->warning("%s: LocationTable argument %s has the wrong type",
                  id().cc(),
                  _locinfo->id().cc());
    _locinfo = 0;
  } else if(_locinfo == 0) {
    return errh->error("no LocationTable argument");
  }
  return 0;
}


Packet *
FixDstLoc::simple_action(Packet *xp)
{
  assert(_loctab); 
  WritablePacket *p = xp->uniqueify();
  grid_nbr_encap *nb = (grid_nbr_encap *) (p->data() + sizeof(click_ether) + sizeof(grid_hdr));
  IPAddress dst(nb->dst_ip);
  grid_location loc;
  nb->dst_loc_good = _loctab.get_location(dst, nb->dst_loc, nb->dst_loc_err);
  return p;
}

ELEMENT_REQUIRES(userlevel LocationTable/)
EXPORT_ELEMENT(FixDstLoc)

