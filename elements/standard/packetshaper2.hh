#ifndef SHAPER2_HH
#define SHAPER2_HH
#include "element.hh"
#include "timer.hh"
#include "ewma.hh"

/*
 * =c
 * PacketShaper2(RATE)
 * =d
 *
 * PacketShaper2 is a pull element that allows a maxmimum of RATE packets per
 * second to pass through. RATE has to be less than 1000000. It has a us
 * granularity, which means it will send at most 1 pkt per us.
 *
 * There are usually Queues both upstream and downstream of PacketShaper2
 * elements.
 *
 * =a PacketShaper
 * =a Shaper
 */

class PacketShaper2 : public Element { protected:

  unsigned _last;
  unsigned _interval;

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
