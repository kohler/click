#ifndef COUNTER_HH
#define COUNTER_HH
#include "element.hh"
#include "ewma.hh"

/* =c
 * Counter([TYPE])
 * =s
 * measures packet count and rate
 * V<measurement>
 * =d
 * Passes packets unchanged from its input to its output,
 * maintaining statistics information about packet count and
 * rate if TYPE is "packets", or byte count and byte rate if
 * TYPE is "bytes". The default TYPE is "packets".
 * =h count read-only
 * Returns the number of packets/bytes that have passed through.
 * =h rate read-only
 * Returns the recent arrival rate (measured by exponential
 * weighted moving average) in packets/bytes per second.
 * =h reset write-only
 * Resets the count and rate to zero.
 */

class Counter : public Element { protected:
 
  bool _bytes;
  int _count;
  RateEWMA _rate;
  
 public:

  Counter();
  
  const char *class_name() const		{ return "Counter"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int count() const				{ return _count; }
  int rate() const				{ return _rate.average(); }
  int rate_scale() const			{ return _rate.scale; }
  int rate_freq() const				{ return _rate.freq(); }
  void reset();
  
  Counter *clone() const			{ return new Counter; }
  int initialize(ErrorHandler *);
  int configure(const Vector<String> &, ErrorHandler *);
  void add_handlers();
  
  /*void push(int port, Packet *);
    Packet *pull(int port);*/
  Packet *simple_action(Packet *);
  
};

#endif
