#ifndef LOCATIONINFO_HH
#define LOCATIONINFO_HH

/*
 * =c
 * LocationInfo(LATITUDE, LONGITUDE)
 * =io
 * None
 * =d
 *
 * LATITUDE and LONGITUDE are in decimal degrees (Real).  Positive is
 * North and East, negative is South and West.
 *
 * =h loc read/write
 * Returns or sets the element's location
 * information, in this format: ``lat, lon''.
 *
 * =a FixSrcLoc */

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

  grid_location get_current_location() { return _loc; }

  void add_handlers();
  int read_args(const Vector<String> &conf, ErrorHandler *errh);

protected:
  grid_location _loc;

};

#endif
