/*
 * locationinfo.{cc,hh} -- element gives the grid node's current location
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
#include "locationinfo.hh"
#include "glue.hh"
#include "confparse.hh"
#include "router.hh"
#include "error.hh"

LocationInfo::LocationInfo() : _loc(32.2816, -64.7685) // my house in bermuda -- doug
{
}

LocationInfo::~LocationInfo()
{
}

int
LocationInfo::read_args(const Vector<String> &conf, ErrorHandler *errh)
{
  int lat_int, lon_int;
  int res = cp_va_parse(conf, this, errh,
			// 5 fractional digits ~= 1 metre precision
			cpReal, "latitude (decimal degrees)", 5, &lat_int,
			cpReal, "longitude (decimal degrees)", 5, &lon_int,
			0);
  float lat = ((float) lat_int) / 100000.0f;
  float lon = ((float) lon_int) / 100000.0f; 
  if (lat > 90 || lat < -90)
    return errh->error("%s: latitude must be between +/- 90 degrees", id().cc());
  if (lon > 180 || lon < -180)
    return errh->error("%s: longitude must be between +/- 180 degrees", id().cc());

  _loc.lat = lat;
  _loc.lon = lon;
  
  return res;
}
int
LocationInfo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return read_args(conf, errh);
}

static String
loc_read_handler(Element *f, void *)
{
  LocationInfo *l = (LocationInfo *) f;
  grid_location loc = l->get_current_location();
  
  const int BUFSZ = 255;
  char buf[BUFSZ];
  int res = snprintf(buf, BUFSZ, "%f, %f\n", (double) loc.lat, (double) loc.lon);
  if (res < 0) {
    click_chatter("LocationInfo read handler buffer too small");
    return String("");
  }
  return String(buf);  
}


static int
loc_write_handler(const String &arg, Element *element,
		  void *, ErrorHandler *errh)
{
  LocationInfo *l = (LocationInfo *) element;
  Vector<String> arg_list;
  cp_argvec(arg, arg_list);
  
  return l->read_args(arg_list, errh);
}

void
LocationInfo::add_handlers()
{
  add_default_handlers(true);
  add_write_handler("loc", loc_write_handler, (void *) 0);
  add_read_handler("loc", loc_read_handler, (void *) 0);
}

EXPORT_ELEMENT(LocationInfo)
