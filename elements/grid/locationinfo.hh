#ifndef LOCATIONINFO_HH
#define LOCATIONINFO_HH

/*
 * =c
 * LocationInfo(LATITUDE, LONGITUDE, [ MOVE ])
 * =io
 * None
 * =d
 *
 * LATITUDE and LONGITUDE are in decimal degrees (Real).  Positive is
 * North and East, negative is South and West.
 *
 * If the optional move parameter is non-zero, the node will move
 * randomly at a few meters per second.
 *
 * =h loc read/write
 * Returns or sets the element's location
 * information, in this format: ``lat, lon''.
 *
 * =a
 * FixSrcLoc */

#include "element.hh"
#include "grid.hh"

class LocationInfo : public Element {
  
public:
  LocationInfo();
  ~LocationInfo();

  const char *class_name() const { return "LocationInfo"; }

  LocationInfo *clone() const { return new LocationInfo; }
  int configure(const Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  grid_location get_current_location();

  void add_handlers();
  int read_args(const Vector<String> &conf, ErrorHandler *errh);

protected:

  int _move;    // Should we move?
  double _lat0; // Where we started.
  double _lon0;
  double _t0;   // When we started.
  double _t1;   // When we're to pick new velocities.
  double _vlat; // Latitude velocity (in degrees).
  double _vlon; // Longitude velocity.

  double now();
  double xlat();
  double xlon();
  double uniform();
  virtual void choose_new_leg(double *, double *, double *);
};

#endif
