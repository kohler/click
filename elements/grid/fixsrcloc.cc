/*
 * fixsrcloc.{cc,hh} -- element sets Grid source location.
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
#include "fixsrcloc.hh"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "grid.hh"
#include "router.hh"
#include "click_ether.h"

FixSrcLoc::FixSrcLoc() : Element(1, 1), _locinfo(0)
{
}

FixSrcLoc::~FixSrcLoc()
{
}

FixSrcLoc *
FixSrcLoc::clone() const
{
  return new FixSrcLoc;
}

int
FixSrcLoc::configure(const Vector<String> &, ErrorHandler *)
{
  return 0;
}

int
FixSrcLoc::initialize(ErrorHandler *errh)
{
  /*
   * Try to find a LocationInfo element
   */
  for (int fi = 0; fi < router()->nelements() && !_locinfo; fi++) {
    Element *f = router()->element(fi);
    LocationInfo *lr = (LocationInfo *)f->cast("LocationInfo");
    if (lr != 0)
      _locinfo = lr;
  }

  if (_locinfo == 0)
    return errh->error("could not find a LocationInfo element");

  return 0;
}


Packet *
FixSrcLoc::simple_action(Packet *p)
{
  assert(_locinfo);
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether) + sizeof(grid_hdr));
  gh->loc = _locinfo->get_current_location();
  gh->loc.lat = (float) htonl((unsigned long) gh->loc.lat);
  gh->loc.lon = (float) htonl((unsigned long) gh->loc.lon);
  return p;
}

EXPORT_ELEMENT(FixSrcLoc)
