/*
 * filterbyrange.{cc,hh} -- Grid packet filtering element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#include <stddef.h>
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "filterbyrange.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"
#include "click_ether.h"
#include "glue.hh"
#include <math.h>

FilterByRange::FilterByRange() : Element(1, 2), _locinfo(0)
{
}

FilterByRange::~FilterByRange()
{
}

int
FilterByRange::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "range (metres)", &_range,
			0);

  if (_range < 0)
    return errh->error("range must be positive");
  return res;
}

int
FilterByRange::initialize(ErrorHandler *errh)
{
  /*
   * Try to find a LocationInfo element
   */
  for (int fi = 0; fi < router()->nelements() && !_locinfo; fi++) {
    Element *f = router()->element(fi);
    LocationInfo *lr = (LocationInfo *)f->cast("LocationInfo");
    if (lr != 0)
      _locinfo = lr;
  }

  if (_locinfo == 0)
    return errh->error("could not find a LocationInfo element");

  return 0;
}

void
FilterByRange::push(int, Packet *p)
{
  assert(p);
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  grid_location remote_loc(gh->loc);

  remote_loc.ntohloc();

  assert(_locinfo);
  grid_location our_loc = _locinfo->get_current_location();
  double dist = calc_range(our_loc, remote_loc);
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

#define GRID_RAD_PER_DEG 0.017453293 // from xcalc
#define GRID_EARTH_RADIUS 6378156 // metres XXX do i believe this?

#define sign(x) (((x) < 0) ? -1 : 1)

double
FilterByRange::calc_range(grid_location l1, grid_location l2)
{
  /*
   * Calculate great circle distance between two points on earth.
   * Assume all angles are valid latitude or longitudes.
   */

  // convert degrees to radians
  double l1_lat = l1.lat * GRID_RAD_PER_DEG;
  double l1_lon = l1.lon * GRID_RAD_PER_DEG;
  double l2_lat = l2.lat * GRID_RAD_PER_DEG;
  double l2_lon = l2.lon * GRID_RAD_PER_DEG;

  double diff_lon;
  if (sign(l1_lon) == sign(l2_lon))
    diff_lon = fabs(l1_lon - l2_lon);
  else {
    if (sign(l1_lon) < 0)
      diff_lon = l2_lon - l1_lon;
    else
      diff_lon = l1_lon - l2_lon;
  }

  double sin_term = sin(l1_lat) * sin(l2_lat);
  double cos_term = cos(l1_lat) * cos(l2_lat);
  double cos_dl = cos(diff_lon);
  double cos_g_c = sin_term + cos_term*cos_dl; 

  // linux precision issues?
#define EPSILON 1.0e-7
  if ((cos_g_c + 1.0 <= EPSILON) ||
      (cos_g_c - 1.0 >= EPSILON)) {
#if 1
    click_chatter("cos_g_c: %0.30f", cos_g_c);
    click_chatter("sin_term: %0.30f", sin_term);
    click_chatter("cos_term: %0.30f", cos_term);
    click_chatter("cos_dl: %0.30f", cos_dl);
    click_chatter("l1_lat: %0.30f", l1_lat);
    click_chatter("l1_lon: %0.30f", l1_lon);
    click_chatter("l2_lat: %0.30f", l2_lat);
    click_chatter("l2_lon: %0.30f", l2_lon);
    click_chatter("l1.lat: %0.30f", l1.lat);
    click_chatter("l1.lon: %0.30f", l1.lon);
    click_chatter("l2.lat: %0.30f", l2.lat);
    click_chatter("l2.lon: %0.30f", l2.lon);
#endif
    return -1; // bogus angles
  }
  double g_c_angle = acos(cos_g_c);
  return g_c_angle * GRID_EARTH_RADIUS;
}


EXPORT_ELEMENT(FilterByRange)


