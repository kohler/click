#ifndef MOVESIM_HH
#define MOVESIM_HH

/*
 * =c
 * MovementSimulator(T NODE-EL V-LAT V-LON, ...)
 * =io
 * None
 * =d
 * 
 * Generate programmed movement in Click-embedded Grid network
 * simulations.  Each argument tuple specifies a new velocity for the
 * LocationInfo element NODE-EL at time T milliseconds, with new
 * latitude velocity of V-LAT degree per second, and new longitude
 * velocity of V-LON degrees per second.
 *
 * =a
 * LocationInfo */

#include "element.hh"
#include "grid.hh"
#include "timer.hh"
#include "vector.hh"
#include "locationinfo.hh"

class MovementSimulator : public Element {
  
public:
  MovementSimulator();
  ~MovementSimulator();

  const char *class_name() const { return "MovementSimulator"; }

  MovementSimulator *clone() const { return new MovementSimulator; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();
  int read_args(const Vector<String> &conf, ErrorHandler *errh);

private:
  Timer _event_timer;

  struct node_event {
    LocationInfo *loc_el;
    double v_lat;
    double v_lon;
    node_event() : loc_el(0) { }
    node_event(LocationInfo *el, double vlat, double vlon) : 
      loc_el(el), v_lat(vlat), v_lon(vlon) { }
  };

  struct event_entry {
    unsigned long t; // relative time in msecs
    Vector<node_event> nodes;
    event_entry *next;
  };

  event_entry *_events; // sentinel for events list

  static void event_hook(unsigned long);
  bool find_entry(unsigned int t, event_entry **retval);
};

#endif
