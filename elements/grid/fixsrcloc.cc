/*
 * fixsrcloc.{cc,hh} -- element sets Grid source location.
 * Douglas S. J. De Couto
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
#include "fixsrcloc.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include "grid.hh"
#include <click/router.hh>
#include <click/click_ether.h>

FixSrcLoc::FixSrcLoc() : Element(1, 1), _locinfo(0)
{
  MOD_INC_USE_COUNT;
}

FixSrcLoc::~FixSrcLoc()
{
  MOD_DEC_USE_COUNT;
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
                        cpElement, "GridLocationInfo element", &_locinfo,
			0);
  return res;
}

int
FixSrcLoc::initialize(ErrorHandler *errh)
{
  if(_locinfo && _locinfo->cast("GridLocationInfo") == 0){
    errh->warning("%s: GridLocationInfo argument %s has the wrong type",
                  id().cc(),
                  _locinfo->id().cc());
    _locinfo = 0;
  } else if(_locinfo == 0){
    return errh->error("no GridLocationInfo argument");
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

ELEMENT_REQUIRES(userlevel GridLocationInfo)
EXPORT_ELEMENT(FixSrcLoc)

