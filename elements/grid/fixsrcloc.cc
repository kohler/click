/*
 * fixsrcloc.{cc,hh} -- element sets Grid source location.
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
  WritablePacket *p = xp->uniqueify();
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  gh->tx_loc = _locinfo->get_current_location(&gh->loc_seq_no);
  gh->tx_loc_seq_no = htonl(gh->loc_seq_no);
  gh->tx_loc_err = 0;
  gh->tx_loc_good = true;
  // only fill in packet originator info if we are originating...
  if (gh->ip == gh->tx_ip) {
    // click_chatter("FixSrcLoc %s: rewriting gh->loc info", id().cc());
    gh->loc = gh->tx_loc;
    gh->loc_seq_no = gh->tx_loc_seq_no;
    gh->loc_err = gh->tx_loc_err;
    gh->loc_good = gh->tx_loc_good;
  }
  return p;
}

ELEMENT_REQUIRES(userlevel LocationInfo)
EXPORT_ELEMENT(FixSrcLoc)

