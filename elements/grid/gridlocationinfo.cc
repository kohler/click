/*
 * gridlocationinfo.{cc,hh} -- element gives the grid node's current location
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
#include "grid.hh"
#include "gridlocationinfo.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <math.h>
CLICK_DECLS

GridLocationInfo::GridLocationInfo() : _seq_no(0), _logging_timer(logging_hook, this)
{
  _move = 0;
  _lat0 = 32.2816;  // Doug's house in Bermuda.
  _lon0 = -64.7685;
  _h0 = 0;
  _t0 = 0;
  _t1 = 0;
  _vlat = 0;
  _vlon = 0;

  _tag = "<unknown>";

  _loc_err = 0;
  _loc_good = false;
}

GridLocationInfo::~GridLocationInfo()
{
}

void *
GridLocationInfo::cast(const char *n)
{
  if (strcmp(n, "GridLocationInfo") == 0 ||
      strcmp(n, "GridGenericLocInfo") == 0)
    return this;
  return 0;
}

void
GridLocationInfo::logging_hook(Timer *, void *thunk) {
  // extended logging
  GridLocationInfo *l = (GridLocationInfo *) thunk;
  grid_location loc = l->get_current_location();

  const int BUFSZ = 255;
  char buf[BUFSZ];
  int res = snprintf(buf, BUFSZ, "loc %s\n\n", loc.s().c_str());
  if (res < 0) {
    click_chatter("LocationInfo read handler buffer too small");
    return;
  }

  l->_extended_logging_errh->message(buf);
  l->_logging_timer.schedule_after_msec (1000);
}

int
GridLocationInfo::read_args(const Vector<String> &conf, ErrorHandler *errh)
{
  int do_move = 0;
  int lat_int, lon_int;
  int h_int = 0;

  String chan("routelog");
  int res = Args(conf, this, errh)
      // 5 fractional digits ~= 1 metre precision at the equator
      .read_mp("LATITUDE", DecimalFixedPointArg(5), lat_int)
      .read_mp("LONGITUDE", DecimalFixedPointArg(5), lon_int)
      .read_p("HEIGHT", DecimalFixedPointArg(3), h_int)
      .read("MOVESIM", do_move)
      .read("LOC_GOOD", _loc_good)
      .read("ERR_RADIUS", _loc_err)
      .read("LOGCHANNEL", chan)
      .read("TAG", _tag)
      .complete();
  if (res < 0)
    return res;

  double lat = ((double) lat_int) / 1e5;
  double lon = ((double) lon_int) / 1e5;
  double h = ((double) h_int) / 1e3;
  if (lat > 90 || lat < -90)
    return errh->error("%s: latitude must be between +/- 90 degrees", name().c_str());
  if (lon > 180 || lon < -180)
    return errh->error("%s: longitude must be between +/- 180 degrees", name().c_str());

  _lat0 = lat;
  _lon0 = lon;
  _h0 = h;
  _move = do_move;

  _extended_logging_errh = router()->chatter_channel(chan);

  return res;
}
int
GridLocationInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _seq_no++;
  int res = read_args(conf, errh);
  if (res < 0)
    return res;

  _logging_timer.initialize(this);
  _logging_timer.schedule_after_msec(100);

  return res;
}

double
GridLocationInfo::now()
{
  return Timestamp::now().doubleval();
}

double
GridLocationInfo::xlat()
{
  if(_move){
    return(_lat0 + _vlat * (now() - _t0));
  } else {
    return(_lat0);
  }
}

double
GridLocationInfo::xlon()
{
  if(_move){
    return(_lon0 + _vlon * (now() - _t0));
  } else {
    return(_lon0);
  }
}

double
GridLocationInfo::uniform()
{
  double x;

  x = (double)click_random() / 0x7fffffff;
  return(x);
}

// Pick a new place to move to, and a time by which we want
// to arrive there.
// Intended to be overridden.
void
GridLocationInfo::choose_new_leg(double *nlat, double *nlon, double *nt)
{
  *nlat = _lat0 + 0.0001 - (uniform() * 0.0002);
  *nlon = _lon0 + 0.0001 - (uniform() * 0.0002);
  *nt = _t0 + 20 * uniform();
}

grid_location
GridLocationInfo::get_current_location(unsigned int *seq_no)
{
  double t = now();

  if(_move == 1 && t >= _t1){
    _lat0 = xlat();
    _lon0 = xlon();
    _t0 = t;
    double nlat = 0, nlon = 0, nt = 0;
    choose_new_leg(&nlat, &nlon, &nt);
    assert(nt > 0);
    _vlat = (nlat - _lat0) / (nt - _t0);
    _vlon = (nlon - _lon0) / (nt - _t0);
    _t1 = nt;
    _seq_no++;
  }

  if (_move == 2) {
    _lat0 = xlat();
    _lon0 = xlon();
    _t0 = t;
    _seq_no++;
  }

  grid_location gl(xlat(), xlon(), _h0);
  if (seq_no != 0)
    *seq_no = _seq_no;
  return(gl);
}

static String
loc_read_handler(Element *f, void *)
{
  GridLocationInfo *l = (GridLocationInfo *) f;
  grid_location loc = l->get_current_location();

  const int BUFSZ = 255;
  char buf[BUFSZ];
  int res = snprintf(buf, BUFSZ, "%s (err=%hu good=%s seq=%u)\n", loc.s().c_str(),
		     l->loc_err(), (l->loc_good() ? "yes" : "no"), l->seq_no());
  if (res < 0) {
    click_chatter("GridLocationInfo read handler buffer too small");
    return String("");
  }
  return String(buf);
}


static int
loc_write_handler(const String &arg, Element *element,
		  void *, ErrorHandler *errh)
{
  GridLocationInfo *l = (GridLocationInfo *) element;
  Vector<String> arg_list;
  cp_argvec(arg, arg_list);

  l->_seq_no++;
  return l->read_args(arg_list, errh);
}

static String
tag_read_handler(Element *f, void *)
{
  GridLocationInfo *l = (GridLocationInfo *) f;
  return "tag=" + l->_tag + "\n";
}


static int
tag_write_handler(const String &arg, Element *element,
		  void *, ErrorHandler *)
{
  GridLocationInfo *l = (GridLocationInfo *) element;
  l->_tag = arg;
  return 0;
}

void
GridLocationInfo::add_handlers()
{
  add_write_handler("loc", loc_write_handler, 0);
  add_read_handler("loc", loc_read_handler, 0);
  add_write_handler("tag", tag_write_handler, 0);
  add_read_handler("tag", tag_read_handler, 0);
}


void
GridLocationInfo::set_new_dest(double v_lat, double v_lon)
{ /* velocities v_lat and v_lon in degrees per sec */

  if (_move != 2) {
    click_chatter("%s: not configured to accept set_new_dest directives!", name().c_str());
    return;
  }

  double t = now();

  _lat0 = xlat();
  _lon0 = xlon();
  _t0 = t;
  _vlat = v_lat;
  _vlon = v_lon;
}


double
grid_location::calc_range(const grid_location &l1, const grid_location &l2)
{
  /* Assumes all angles are valid latitude or longitudes */

  /*
   * Calculates distance between two 3-D locations by pretending the
   * curved surface of the earth is actually a flat plane.  We can
   * use Euclidean distance, first calculating the great circle
   * distance between two points on earth, then pretending that
   * distance is along a straight line, and treating it as the
   * bottom of a right triangle whose vertical side is the
   * difference in the heights of the two points.  This ought to be
   * pretty much accurate when points are close enough enough
   * together when their heights are important.
   */

  // convert degrees to radians
  double l1_lat = l1.lat() * GRID_RAD_PER_DEG;
  double l1_lon = l1.lon() * GRID_RAD_PER_DEG;
  double l2_lat = l2.lat() * GRID_RAD_PER_DEG;
  double l2_lon = l2.lon() * GRID_RAD_PER_DEG;

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
    click_chatter("l1.lat: %0.30f", l1.lat());
    click_chatter("l1.lon: %0.30f", l1.lon());
    click_chatter("l2.lat: %0.30f", l2.lat());
    click_chatter("l2.lon: %0.30f", l2.lon());
#endif
    return -1; // bogus angles
  }
  double g_c_dist = acos(cos_g_c) * GRID_EARTH_RADIUS;

  double dh = fabs(l1.h() - l2.h());
  double r_squared = dh*dh + g_c_dist*g_c_dist;
  return sqrt(r_squared);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(GridGenericLocInfo)
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridLocationInfo)
