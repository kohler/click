#ifndef STATICLOCATIONINFO_HH
#define STATICLOCATIONINFO_HH

/*
 * =c
 * StaticLocationInfo(LATITUDE, LONGITUDE)
 * =io
 * None
 * =d
 *
 * A subclass of LocationInfo.  LATITUDE and LONGITUDE are in decimal
 * degrees (Real).  Positive is North and East, negative is South and
 * West. */

#include "element.hh"
#include "locationinfo.hh"
#include "grid.hh"

class StaticLocationInfo : public LocationInfo {
public:
  StaticLocationInfo();
  ~StaticLocationInfo();

  const char *class_name() const { return "StaticLocationInfo"; }

  void *cast(const char *name);

  LocationInfo *clone() const { return new StaticLocationInfo; }
  int configure(const Vector<String> &, ErrorHandler *);

};

#endif
