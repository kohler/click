/*
 * filterbyrange.{cc,hh} -- Grid packet filtering element
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

#include <click/config.h>
#include <stddef.h>
#include "filterbyrange.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <clicknet/ether.h>
#include <click/glue.hh>
CLICK_DECLS

FilterByRange::FilterByRange() : _locinfo(0)
{
}

FilterByRange::~FilterByRange()
{
}

int
FilterByRange::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("RANGE", _range)
	.read_mp("LOCINFO", reinterpret_cast<Element *&>(_locinfo))
	.complete();
}

int
FilterByRange::initialize(ErrorHandler *errh)
{
  if(_locinfo && _locinfo->cast("GridLocationInfo") == 0){
    errh->warning("%s: GridLocationInfo argument %s has the wrong type",
                  name().c_str(),
                  _locinfo->name().c_str());
    _locinfo = 0;
  } else if(_locinfo == 0){
    return errh->error("no GridLocationInfo argument");
  }

  return 0;
}

void
FilterByRange::push(int, Packet *p)
{
  assert(p);

  if (_range < 0) { // negative range means: don't filter
    output(0).push(p);
    return;
  }

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  grid_location remote_loc(gh->tx_loc);

  assert(_locinfo);
  grid_location our_loc = _locinfo->get_current_location();
  double dist = grid_location::calc_range(our_loc, remote_loc);
#if 0
  IPAddress tx(gh->tx_ip);
  click_chatter("XXXXX range %s %f", tx.unparse().c_str(), dist);
#endif
  if (dist < 0) {
    click_chatter("bogus location info in grid header");
    output(1).push(p);
    return;
  }
  if (dist < _range)
    output(0).push(p);
  else // ``out of range''
    output(1).push(p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel GridLocationInfo)
EXPORT_ELEMENT(FilterByRange)
