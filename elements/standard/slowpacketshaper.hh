#ifndef SLOWPKTSHAPER_HH
#define SLOWPKTSHAPER_HH
#include "element.hh"
#include "timer.hh"
#include "ewma.hh"

/*
 * =c
 * SlowPacketShaper(RATE)
 * =s shapes traffic to maximum rate (pkt/s) 
 * V<packet scheduling>
 * =d
 *
 * SlowPacketShaper is a pull element that allows a maxmimum of RATE packets
 * per second to pass through. SlowPacketShaper does not use EWMAs to maintain
 * rates (unlike PacketShaper). Timers used for EWMAs do not have the
 * granularity needed for keeping slow rates (e.g. 10 packets per second).
 *
 * There are usually Queues both upstream and downstream of SlowPacketShaper
 * elements.
 *
 * =a PacketShaper, Shaper, RatedSampler
 */

class SlowPacketShaper : public Element { protected:

  unsigned _meter;
  unsigned _ugap;
  unsigned _total;
  struct timeval _start;

 public:
  
  SlowPacketShaper();
  ~SlowPacketShaper();
  
  const char *class_name() const                { return "SlowPacketShaper"; }
  const char *processing() const		{ return PULL; }

  SlowPacketShaper *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *pull(int);
};

#endif
