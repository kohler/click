/*
 * gridlocationinfo2.{cc,hh} -- element gives the grid node's current location
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2003 Massachusetts Institute of Technology
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
#include "gridlocationinfo2.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

GridLocationInfo2::GridLocationInfo2() 
  : _seq_no(0), _tag("<unknown>"),
    _loc_good(false), _loc_err(0), _loc(0L, 0L, 0L)
{
  MOD_INC_USE_COUNT;
}

GridLocationInfo2::~GridLocationInfo2()
{
  MOD_DEC_USE_COUNT;
}

void *
GridLocationInfo2::cast(const char *n)
{
  if (strcmp(n, "GridLocationInfo2") == 0 ||
      strcmp(n, "GridGenericLocInfo") == 0)
    return this;
  return 0;
}

int
GridLocationInfo2::read_args(const Vector<String> &conf, ErrorHandler *errh)
{
  int lat, lon, h = 0;
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "latitude (integer milliseconds)", &lat,
			cpInteger, "longitude (integer milliseconds)", &lon,
			cpOptional,
			cpInteger, "height (integer millemetres)", &h,
			cpKeywords,
			"LOC_GOOD", cpBool, "Is our location information valid?", &_loc_good,
			"ERR_RADIUS", cpUnsignedShort, "Location error radius, in metres", &_loc_err,
			"TAG", cpString, "location tag", &_tag,
			0);
  if (res < 0)
    return res;

  const int msec_per_deg = 1000 * 60 * 60;

  if (lat > 90*msec_per_deg || lat < -90*msec_per_deg)
    return errh->error("%s: latitude arg %d must be between +/- 90 degrees (+/- %d milliseconds)", id().cc(), lat, 90*msec_per_deg);
  if (lon > 180*msec_per_deg || lon < -180*msec_per_deg)
    return errh->error("%s: longitude arg %d must be between +/- 180 degrees (+/- %d milliseconds)", id().cc(), lon, 180*msec_per_deg);

  _loc = grid_location((long) lat, (long) lon, (long) h);
  return res;
}
int
GridLocationInfo2::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _seq_no++;
  int res = read_args(conf, errh);
  if (res < 0)
    return res;
  
  return res;
}

grid_location
GridLocationInfo2::get_current_location(unsigned int *seq_no)
{
  if (seq_no != 0)
    *seq_no = _seq_no;
  return _loc;
}

static String
loc_read_handler(Element *f, void *)
{
  GridLocationInfo2 *l = (GridLocationInfo2 *) f;
  grid_location loc = l->get_current_location();
  
  const int BUFSZ = 255;
  char buf[BUFSZ];
  int res = snprintf(buf, BUFSZ, "%s (err=%hu good=%s seq=%u)\n", loc.s().cc(),
		     l->loc_err(), (l->loc_good() ? "yes" : "no"), l->seq_no());
  if (res < 0) {
    click_chatter("GridLocationInfo2 read handler buffer too small");
    return String("");
  }
  return String(buf);  
}


static int
loc_write_handler(const String &arg, Element *element,
		  void *, ErrorHandler *errh)
{
  GridLocationInfo2 *l = (GridLocationInfo2 *) element;
  Vector<String> arg_list;
  cp_argvec(arg, arg_list);

  l->_seq_no++;
  return l->read_args(arg_list, errh);
}

static String
tag_read_handler(Element *f, void *)
{
  GridLocationInfo2 *l = (GridLocationInfo2 *) f;
  return "tag=" + l->_tag + "\n";
}


static int
tag_write_handler(const String &arg, Element *element,
		  void *, ErrorHandler *)
{
  GridLocationInfo2 *l = (GridLocationInfo2 *) element;
  l->_tag = arg;
  return 0;
}

void
GridLocationInfo2::add_handlers()
{
  add_default_handlers(true);
  add_write_handler("loc", loc_write_handler, (void *) 0);
  add_read_handler("loc", loc_read_handler, (void *) 0);
  add_write_handler("tag", tag_write_handler, (void *) 0);
  add_read_handler("tag", tag_read_handler, (void *) 0);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(GridGenericLocInfo)
EXPORT_ELEMENT(GridLocationInfo2)
