#ifndef COUNTER_HH
#define COUNTER_HH
#include "element.hh"
#include "ewma.hh"

/*
 * =c
 * Counter()
 * =d
 * Counts arriving packets. Makes the count available in
 * /proc/click/xxx/count, and the current rate (measured by
 * by exponential weight moving average) in /proc/click/xxx/rate.
 */

class Counter : public Element {
  
  int _count;
  EWMA _rate;
  
 public:

  Counter();
  
  const char *class_name() const		{ return "Counter"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  int count() const				{ return _count; }
  int rate() const				{ return _rate.average(); }
  int rate_scale() const			{ return _rate.scale(); }
  void reset();
  
  Counter *clone() const			{ return new Counter; }
  int initialize(Router *, ErrorHandler *);
  void add_handlers(HandlerRegistry *);
  
  void push(int port, Packet *);
  Packet *pull(int port);
  
};

#endif
