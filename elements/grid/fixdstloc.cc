/*
 * fixdstloc.{cc,hh} -- element sets Grid destination location.
 * Douglas S. J. De Couto
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
  int err;
  bool found = _loctab.get_location(dst, loc, err);
  nb->dst_loc = loc;
  nb->dst_loc_err = err;
  return p;
}

ELEMENT_REQUIRES(userlevel LocationTable/)
EXPORT_ELEMENT(FixDstLoc)

