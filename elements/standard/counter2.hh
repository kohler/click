#ifndef COUNTER2_HH
#define COUNTER2_HH
#include "element.hh"
#include "ewma.hh"
#include "timer.hh"

/*
 * =c
 * Counter2()
 * =s
 * measures historical packet count and rate
 * V<measurement>
 * =d
 *
 * Passes packets unchanged from its input to its output, maintaining
 * statistics information about packet count and packet rate using historical
 * average.
 *
 * =h count read-only
 * Returns the number of packets that have passed through.
 *
 * =h rate read-only
 * Returns packet arrival rate.
 *
 * =h reset write-only
 * Resets the count and rate to zero.
 */

class Counter2 : public Element { protected:
  
  int _count;
  unsigned long _first;
  unsigned long _last;
  
 public:

  Counter2();
  
  const char *class_name() const		{ return "Counter2"; }
  const char *processing() const		{ return AGNOSTIC; }

  int count() const				{ return _count; }
  unsigned long first() const			{ return _first; }
  unsigned long last() const			{ return _last; }
  void reset();
  
  Counter2 *clone() const			{ return new Counter2; }
  int initialize(ErrorHandler *);
  void add_handlers();
  
  Packet *simple_action(Packet *);
};

#endif
