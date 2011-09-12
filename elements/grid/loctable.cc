/*
 * loctable.{cc,hh} -- element maps IP addresses to Grid locations
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
#include "loctable.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

LocationTable::LocationTable()
{
}

LocationTable::~LocationTable()
{
}

int
LocationTable::read_args(const Vector<String> &conf, ErrorHandler *errh)
{
  for (int i = 0; i < conf.size(); i++) {
    IPAddress ip;
    int lat, lon;
    int err;
    int res = Args(this, errh).push_back_words(conf[i])
	.read_mp("IP", ip)
	.read_mp("LATITUDE", DecimalFixedPointArg(7), lat)
	.read_mp("LONGITUDE", DecimalFixedPointArg(7), lon)
	.read_mp("ERROR_RADIUS", err)
	.complete();
    if (res < 0)
      return -1;

    grid_location loc((double) lat /  1.0e7, (double) lon /  1.0e7);
    bool is_new = _locs.insert(ip, entry(loc, err));
    if (!is_new)
      return errh->error("LocationTable %s: %s already has a location mapping",
			 name().c_str(), ip.unparse().c_str());
  }
  return 0;
}
int
LocationTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  return read_args(conf, errh);
}


static String
table_read_handler(Element *f, void *)
{
  LocationTable *l = (LocationTable *) f;

  String res("");
  const int BUFSZ = 255;
  char buf[BUFSZ];
  for (LocationTable::Table::iterator iter = l->_locs.begin(); iter.live(); iter++) {
    const LocationTable::entry &ent = iter.value();
    int r = snprintf(buf, BUFSZ, "%s loc=%s err=%d\n", iter.key().unparse().c_str(), ent.loc.s().c_str(), ent.err);
    if (r < 0) {
      click_chatter("LocationTable %s read handler buffer too small", l->name().c_str());
      return String("");
    }
    res += buf;
  }
  return res;
}

bool
LocationTable::get_location(IPAddress ip, grid_location &loc, int &err_radius)
{
  entry *l2 = _locs.findp(ip);
  if (!l2)
    return false;
  loc = l2->loc;
  err_radius = l2->err;
  return true;
}


static int
loc_write_handler(const String &arg, Element *element,
		  void *, ErrorHandler *errh)
{
  LocationTable *l = (LocationTable *) element;
  int lat, lon;
  IPAddress ip;
  int err;
  int res = Args(l, errh).push_back_words(arg)
      .read_mp("IP", ip)
      .read_mp("LATITUDE", DecimalFixedPointArg(7), lat)
      .read_mp("LONGITUDE", DecimalFixedPointArg(7), lon)
      .read_mp("ERROR_RADIUS", err)
      .complete();
  if (res < 0)
    return -1;
  grid_location loc((double) lat /  1.0e7, (double) lon /  1.0e7);
  l->_locs.insert(ip, LocationTable::entry(loc, err));
  return 0;
}

void
LocationTable::add_handlers()
{
  add_write_handler("loc", loc_write_handler, 0);
  add_read_handler("table", table_read_handler, 0);
}



ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LocationTable)
CLICK_ENDDECLS
