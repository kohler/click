#ifndef LOCATIONINFO_HH
#define LOCATIONINFO_HH

/*
 * =c
 * LocationInfo
 * =io
 * None
 * =d
 * Not sure yet!  A part of Grid.
 *
 * =a StaticLocationInfo
 */

#include "element.hh"
#include "grid.hh"

class LocationInfo : public Element {
public:
  LocationInfo();
  ~LocationInfo();

  const char *class_name() const { return "LocationInfo"; }

  LocationInfo *clone() const { return new LocationInfo; }
  
  grid_location get_current_location() { return _loc; }

protected:
  grid_location _loc;
};

#endif
