#ifndef GRIDLOCATIONINFO_HH
#define GRIDLOCATIONINFO_HH

/*
 * =c
 * GridLocationInfo(LATITUDE, LONGITUDE, [ MOVE ])
 * =io
 * None
 * =d
 *
 * LATITUDE and LONGITUDE are in decimal degrees (Real).  Positive is
 * North and East, negative is South and West.
 *
 * Only 5 digits of precision are used, the rest are ignored and
 * rounding is not performed on the remaining digits.  This should
 * provide about 1 metre of precision near the equator.
 *
 * If the optional move parameter is 1, the node will move
 * randomly at a few meters per second.
 *
 * If the optional move parameter is 2, the node will accept external
 * ``set_new_dest'' directives for setting its speed etc.
 *
 * =h loc read/write
 * Returns or sets the element's location
 * information, in this format: ``lat, lon''.
 *
 * =a
 * FixSrcLoc */

#include <click/element.hh>
#include "grid.hh"

class GridLocationInfo : public Element {
  
public:
  GridLocationInfo();
  ~GridLocationInfo();

  const char *class_name() const { return "GridLocationInfo"; }

  GridLocationInfo *clone() const { return new GridLocationInfo; }
  int configure(const Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  // seq_no is incremented when location changes ``enough to make a
  // difference''
  grid_location get_current_location(unsigned int *seq_no = 0);

  void add_handlers();
  int read_args(const Vector<String> &conf, ErrorHandler *errh);

  void set_new_dest(double v_lat, double v_lon);

  unsigned int _seq_no;
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
