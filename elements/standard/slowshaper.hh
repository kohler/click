#ifndef SLOWSHAPER_HH
#define SLOWSHAPER_HH
#include "element.hh"
#include "timer.hh"
#include "ewma.hh"

/*
 * =c
 * SlowShaper(RATE)
 * =s shapes traffic to maximum rate (pkt/s) 
 * V<packet scheduling>
 * =d
 *
 * SlowShaper is a pull element that allows a maxmimum of RATE packets per
 * second to pass through. SlowShaper does not use EWMAs to maintain rates
 * (unlike Shaper). Timers used for EWMAs do not have the granularity needed
 * for keeping slow rates (e.g. 10 packets per second).
 *
 * There are usually Queues both upstream and downstream of SlowShaper
 * elements.
 *
 * =a Shaper, BandwidthShaper, RatedSplitter */

class SlowShaper : public Element { protected:

  unsigned _meter;
  unsigned _ugap;
  unsigned _total;
  struct timeval _start;

 public:
  
  SlowShaper();
  ~SlowShaper();
  
  const char *class_name() const                { return "SlowShaper"; }
  const char *processing() const		{ return PULL; }

  SlowShaper *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *pull(int);
  
};

#endif
