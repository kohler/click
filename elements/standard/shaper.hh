#ifndef SHAPER_HH
#define SHAPER_HH
#include "element.hh"
#include "timer.hh"
#include "ewma.hh"

/*
 * =c
 * Shaper(RATE)
 * =d
 * Shaper is a pull element that allows a maxmimum of RATE
 * bytes per second to pass through. It measures RATE using
 * an exponential weighted moving average.
 *
 * There are usually Queues both upstream and downstream
 * of Shaper elements.
 * =a PacketShaper
 */

class Shaper : public Element { protected:

  EWMA _rate;
  
  int _meter1;

  Element *_puller1;
  Vector<Element *> _pullers;

 public:
  
  Shaper();
  ~Shaper();
  
  const char *class_name() const                { return "Shaper"; }
  Processing default_processing() const       { return PULL; }

  int rate() const				{ return _rate.average(); }
  int rate_scale() const			{ return _rate.scale(); }
  
  Shaper *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  int initialize(Router *, ErrorHandler *);
  void add_handlers(HandlerRegistry *);

  Packet *pull(int);
  
};

#endif
