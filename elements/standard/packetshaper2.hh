#ifndef SHAPER2_HH
#define SHAPER2_HH
#include "element.hh"
#include "timer.hh"
#include "ewma.hh"

/*
 * =c
 * PacketShaper2(RATE)
 * =s shapes traffic to maximum rate (pkt/s) 
 * V<packet scheduling>
 * =d
 *
 * PacketShaper2 is a pull element that allows a maxmimum of RATE packets per
 * second to pass through. Differs from PacketShaper in that RATEs are not
 * kept using moving averages.
 *
 * There are usually Queues both upstream and downstream of PacketShaper2
 * elements.
 *
 * =a PacketShaper, Shaper, RatedSampler
 */

class PacketShaper2 : public Element { protected:

  unsigned _meter;
  unsigned _ugap;
  unsigned _total;
  struct timeval _start;

 public:
  
  PacketShaper2();
  ~PacketShaper2();
  
  const char *class_name() const                { return "PacketShaper2"; }
  const char *processing() const		{ return PULL; }

  PacketShaper2 *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *pull(int);
};

#endif
