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
LocationInfo::configure(const Vector<String> &conf, ErrorHandler *errh)
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
    return errh->error("latitude must be between +/- 90 degrees");
  if (lon > 180 || lon < -180)
    return errh->error("longitude must be between +/- 180 degrees");

  _loc.lat = lat;
  _loc.lon = lon;
  
  return res;
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


#if 0
static int
loc_write_handler(const String &arg, Element *element,
		  void *vno, ErrorHandler *errh)
{
  // XXX can't get the reconfigure_write_handler of Element to work,
  // or maybe i just don't know how to format my args?
  LocationInfo *l = (LocationInfo *) element;
  return 0;
}
#endif

void
LocationInfo::add_handlers()
{
  add_write_handler("loc", reconfigure_write_handler, (void *) 2);
  add_read_handler("loc", loc_read_handler, (void *) 0);
}

EXPORT_ELEMENT(LocationInfo)
