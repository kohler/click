/*
 * staticlocationinfo.{cc,hh} -- element gives the grid node's current
 * location, as statically configured.
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "staticlocationinfo.hh"
#include "glue.hh"
#include "confparse.hh"
#include "router.hh"
#include "error.hh"

StaticLocationInfo::StaticLocationInfo()
{
}


StaticLocationInfo::~StaticLocationInfo()
{
}


int
StaticLocationInfo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int lat_int, lon_int;
  int res = cp_va_parse(conf, this, errh,
			// 5 fractional digits ~= 1 metre precision
			cpReal, "latitude (decimal degrees)", 5, &lat_int,
			cpReal, "longitude (decimal degrees)", 5, &lon_int,
			0);
  _loc.lat = ((float) lat_int) / 100000.0f;
  _loc.lon = ((float) lon_int) / 100000.0f;
  
  if (_loc.lat > 90 || _loc.lat < -90)
    return errh->error("latitude must be between +/- 90 degrees");
  if (_loc.lon > 180 || _loc.lon < -180)
    return errh->error("longitude must be between +/- 180 degrees");

  return res;
}


void *
StaticLocationInfo::cast(const char *name)
{
  const char *my_name = class_name();
  const char *loc_info_name = LocationInfo::class_name();
  if (my_name && name && strcmp(my_name, name) == 0)
    return this;
  else if (loc_info_name && name && strcmp(loc_info_name, name) == 0)
    return this;
  else
    return 0;
}

EXPORT_ELEMENT(StaticLocationInfo)
