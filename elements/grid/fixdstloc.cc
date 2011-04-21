/*
 * fixdstloc.{cc,hh} -- element sets Grid destination location.
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
#include "fixdstloc.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include "grid.hh"
#include <click/router.hh>
#include <clicknet/ether.h>
CLICK_DECLS

FixDstLoc::FixDstLoc() : _loctab(0)
{
}

FixDstLoc::~FixDstLoc()
{
}

int
FixDstLoc::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("LOCTABLE", ElementCastArg("LocationTable"), _loctab)
	.complete();
}


Packet *
FixDstLoc::simple_action(Packet *xp)
{
  assert(_loctab);
  grid_hdr *gh = (grid_hdr *) (xp->data() + sizeof(click_ether));
  if (gh->type != grid_hdr::GRID_NBR_ENCAP) {
    click_chatter("FixDstLoc %s: not an encapsulated data packet; not modifying it\n", name().c_str());
    return xp;
  }

#ifndef SMALL_GRID_HEADERS
  WritablePacket *p = xp->uniqueify();
  grid_nbr_encap *nb = (grid_nbr_encap *) (p->data() + sizeof(click_ether) + sizeof(grid_hdr));
  IPAddress dst(nb->dst_ip);
  grid_location loc;
  nb->dst_loc_good = _loctab.get_location(dst, nb->dst_loc, nb->dst_loc_err);

  return p;
#else
  return xp;
#endif
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LocationTable)
EXPORT_ELEMENT(FixDstLoc)
