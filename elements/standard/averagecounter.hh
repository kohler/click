#ifndef AVERAGECOUNTER_HH
#define AVERAGECOUNTER_HH
#include "element.hh"
#include "ewma.hh"
#include "timer.hh"

/*
 * =c
 * AverageCounter()
 * =s
 * measures historical packet count and rate
 * V<measurement>
 * =d
 *
 * Passes packets unchanged from its input to its output, maintaining
 * statistics information about packet count and packet rate using a strict
 * average.
 *
 * =h count read-only
 * Returns the number of packets that have passed through.
 *
 * =h average read-only
 * Returns packet arrival rate.
 *
 * =h reset write-only
 * Resets the count and rate to zero.
 */

class AverageCounter : public Element { protected:
  
  int _count;
  unsigned long _first;
  unsigned long _last;
  
 public:

  AverageCounter();
  
  const char *class_name() const		{ return "AverageCounter"; }
  const char *processing() const		{ return AGNOSTIC; }

  int count() const				{ return _count; }
  unsigned long first() const			{ return _first; }
  unsigned long last() const			{ return _last; }
  void reset();
  
  AverageCounter *clone() const			{ return new AverageCounter; }
  int initialize(ErrorHandler *);
  void add_handlers();
  
  Packet *simple_action(Packet *);
};

#endif
