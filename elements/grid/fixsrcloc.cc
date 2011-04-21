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
#include <click/args.hh>
#include <click/error.hh>
#include "grid.hh"
#include <click/router.hh>
#include <clicknet/ether.h>
#include "gridgenericlocinfo.hh"
CLICK_DECLS

FixSrcLoc::FixSrcLoc() : _locinfo(0)
{
}

FixSrcLoc::~FixSrcLoc()
{
}

int
FixSrcLoc::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("LOCINFO", ElementCastArg("GridGenericLocInfo"), _locinfo)
	.complete();
}


Packet *
FixSrcLoc::simple_action(Packet *xp)
{
  assert(_locinfo);
  WritablePacket *p = xp->uniqueify();
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  gh->tx_loc = _locinfo->get_current_location(&gh->loc_seq_no);
  gh->tx_loc_seq_no = htonl(_locinfo->seq_no());
  gh->tx_loc_err = htons(_locinfo->loc_err());
  gh->tx_loc_good = _locinfo->loc_good();
  // only fill in packet originator info if we are originating...
  if (gh->ip == gh->tx_ip) {
    gh->loc = gh->tx_loc;
    gh->loc_seq_no = gh->tx_loc_seq_no;
    gh->loc_err = gh->tx_loc_err;
    gh->loc_good = gh->tx_loc_good;
  }
  return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(GridGenericLocInfo)
EXPORT_ELEMENT(FixSrcLoc)
