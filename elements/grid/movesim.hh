#ifndef MOVESIM_HH
#define MOVESIM_HH

/*
 * =c
 * MovementSimulator(T NODE-EL V-LAT V-LON, ...)
 * =s Grid
 * =io
 * None
 * =d
 *
 * Generate programmed movement in Click-embedded Grid network
 * simulations.  Each argument tuple specifies a new velocity for the
 * GridLocationInfo element NODE-EL at time T milliseconds, with new
 * latitude velocity of V-LAT degree per second, and new longitude
 * velocity of V-LON degrees per second.
 *
 * =a
 * GridLocationInfo */

#include <click/element.hh>
#include "grid.hh"
#include <click/timer.hh>
#include <click/vector.hh>
#include "elements/grid/gridlocationinfo.hh"
CLICK_DECLS

class MovementSimulator : public Element {

public:
  MovementSimulator() CLICK_COLD;
  ~MovementSimulator() CLICK_COLD;

  const char *class_name() const { return "MovementSimulator"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  int read_args(const Vector<String> &conf, ErrorHandler *errh);

private:
  Timer _event_timer;

  struct node_event {
    GridLocationInfo *loc_el;
    double v_lat;
    double v_lon;
    node_event() : loc_el(0) { }
    node_event(GridLocationInfo *el, double vlat, double vlon) :
      loc_el(el), v_lat(vlat), v_lon(vlon) { }
  };

  struct event_entry {
    unsigned long t; // relative time in msecs
    Vector<node_event> nodes;
    event_entry *next;
  };

  event_entry *_events; // sentinel for events list

  static void event_hook(Timer *, void *);
  bool find_entry(unsigned int t, event_entry **retval);
};

CLICK_ENDDECLS
#endif
