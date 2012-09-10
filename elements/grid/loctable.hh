#ifndef LOCTAB_HH
#define LOCTAB_HH

/*
 * =c
 * LocationTable(IP LAT LON ERR, ...)
 * =s Grid
 * =io
 * None
 * =d
 *
 * LAT and LON are in decimal degrees (Real).  Positive is North and
 * East, negative is South and West.  ERR is the integer error radius,
 * in meters.  A negative rror radius means don't ever believe this
 * entry.  There can be multiple arguments, but each argument's IP
 * address must be unique.
 *
 *
 * =h loc write
 * Sets a specified node's location
 * information, in this format: ``ip lat lon error''.
 *
 * =h table read
 * Returns the whole table, each line of the form ``ip lat lon error''
 *
 * =a
 * FixDstLoc */

#include <click/element.hh>
#include "grid.hh"
#include <click/bighashmap.hh>
CLICK_DECLS

class LocationTable : public Element {

public:
  LocationTable() CLICK_COLD;
  ~LocationTable() CLICK_COLD;

  const char *class_name() const { return "LocationTable"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const { return true; }

  bool get_location(IPAddress ip, grid_location &loc, int &err_radius);

  void add_handlers() CLICK_COLD;
  int read_args(const Vector<String> &conf, ErrorHandler *errh);



  struct entry {
    grid_location loc;
    int err;
    entry(grid_location l, int e) : loc(l), err(e) { }
    entry() : err(-1) { }
  };
  typedef HashMap<IPAddress, entry> Table;
  Table _locs;

private:



};

CLICK_ENDDECLS
#endif
