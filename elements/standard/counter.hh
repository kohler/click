#ifndef COUNTER_HH
#define COUNTER_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/llrpc.h>

/* =c
 * Counter([TYPE])
 * =s measurement
 * measures packet count and rate
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
 * =h CLICK_LLRPC_GET_RATE llrpc
 * Argument is a pointer to an integer that must be 0.  Returns the recent
 * arrival rate (measured by exponential weighted moving average) in
 * packets/bytes per second. 
 */

class Counter : public Element { protected:
 
  bool _bytes;
  int _count;
  RateEWMA _rate;
  
 public:

  Counter();
  ~Counter();
  
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
  int llrpc(unsigned, void *);
  
  /*void push(int port, Packet *);
    Packet *pull(int port);*/
  Packet *simple_action(Packet *);
  
};

#endif
