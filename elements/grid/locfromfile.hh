#ifndef LocFromFile_hh
#define LocFromFile_hh 1

/*
 * =c
 * LocFromFile(filename)
 * =s Grid
 * =d
 * Pretends to be a GridLocationInfo element, but actually reads the
 * locations from a file. Each line of the file looks like
 *
 *   interval lat lon
 *
 * This means means "spend interval seconds moving to lat/lon."
 *
 * Here's a reasonable test file that keeps the node more or
 * less within 250 meters of 0,0:
 *
 * 1 0 0
 * 10 .002 .002
 * 20 0 .003
 * 30 0 0
 *
 * =a
 * GridLocationInfo
 */

#include <click/element.hh>
#include "elements/grid/gridlocationinfo.hh"
CLICK_DECLS

class LocFromFile : public GridLocationInfo {
public:
  LocFromFile() CLICK_COLD;
  ~LocFromFile() CLICK_COLD;

  const char *class_name() const { return "LocFromFile"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const { return true; }
  virtual void *cast(const char *);

protected:
  virtual void choose_new_leg(double *, double *, double *);

private:
  struct delta {
    double interval;
    double lat;
    double lon;
  };
  Vector<delta> _deltas;
  int _next;
};

CLICK_ENDDECLS
#endif
