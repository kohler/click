#ifndef COUNTER_HH
#define COUNTER_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/llrpc.h>

/*
=c
Counter()

=s measurement
measures packet count and rate

=d

Passes packets unchanged from its input to its output, maintaining statistics
information about packet count and packet rate.

=h count read-only
Returns the number of packets that have passed through.

=h byte_count read-only
Returns the number of packets that have passed through.

=h rate read-only
Returns the recent arrival rate (measured by exponential
weighted moving average) in packets/bytes per second.

=h reset write-only
Resets the counts and rates to zero.

=h CLICK_LLRPC_GET_RATE llrpc

Argument is a pointer to an integer that must be 0.  Returns the recent
arrival rate (measured by exponential weighted moving average) in
packets per second. 

=h CLICK_LLRPC_GET_COUNT llrpc

Argument is a pointer to an integer that must be 0 (packet count) or 1 (byte
count). Returns the current packet or byte count.

=h CLICK_LLRPC_GET_COUNTS llrpc

Argument is a pointer to an array of 16 integers. First integer is number of
counts to pull. Remaining integers must each be 0 (packet count) or 1 (byte
count). Stores the corresponding count on top of each integer.

*/

class Counter : public Element { public:

  Counter();
  ~Counter();
  
  const char *class_name() const		{ return "Counter"; }
  const char *processing() const		{ return AGNOSTIC; }

  void reset();
  
  Counter *clone() const			{ return new Counter; }
  int initialize(ErrorHandler *);
  void add_handlers();
  int llrpc(unsigned, void *);
  
  Packet *simple_action(Packet *);

 private:
   
  unsigned _count;
  unsigned _byte_count;
  RateEWMA _rate;

  static String read_handler(Element *, void *);
  
};

#endif
