#ifndef COUNTER_HH
#define COUNTER_HH
#include "element.hh"
#include "ewma.hh"

/*
 * =c
 * Counter()
 * =d
 * Passes packets unchanged from its input to its output,  maintaining
 * statistics information about packet count and rate.
 * =h count read-only
 * Returns the number of packets that have passed through.
 * =h rate read-only
 * Returns the recent packet arrival rate (measured by exponential
 * weighted moving average) in packets per second.
 * =h reset write-only
 * Resets the count and rate to zero.
 */

class Counter : public Element { protected:
  
  int _count;
  EWMA _rate;
  
 public:

  Counter();
  
  const char *class_name() const		{ return "Counter"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int count() const				{ return _count; }
  int rate() const				{ return _rate.average(); }
  int rate_scale() const			{ return _rate.scale(); }
  void reset();
  
  Counter *clone() const			{ return new Counter; }
  int initialize(ErrorHandler *);
  void add_handlers();
  
  /*void push(int port, Packet *);
    Packet *pull(int port);*/
  Packet *simple_action(Packet *);
  
};

#endif
