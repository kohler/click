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
FixSrcLoc::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
                        cpElement, "LocationInfo element", &_locinfo,
			0);
  return res;
}

int
FixSrcLoc::initialize(ErrorHandler *errh)
{
  if(_locinfo && _locinfo->cast("LocationInfo") == 0){
    errh->warning("%s: LocationInfo argument %s has the wrong type",
                  id().cc(),
                  _locinfo->id().cc());
    _locinfo = 0;
  } else if(_locinfo == 0){
    return errh->error("no LocationInfo argument");
  }

  return 0;
}


Packet *
FixSrcLoc::simple_action(Packet *xp)
{
  assert(_locinfo);
#if 1
  grid_hdr *bitch = (grid_hdr *) (xp->data() + sizeof(click_ether));
  int asshole = ntohs(bitch->total_len);
  click_chatter("fix pre pre ***** %d", asshole);
#endif
  WritablePacket *p = xp->uniqueify();
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
#if 1
  int len1 = ntohs(gh->total_len);
  click_chatter("fix pre XXX total len is %d", len1);
#endif
  gh->loc = _locinfo->get_current_location();
#if 1
  int len = ntohs(gh->total_len);
  click_chatter("fix XXX total len is %d", len);
#endif
  return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FixSrcLoc)
