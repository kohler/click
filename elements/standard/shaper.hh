#ifndef SHAPER_HH
#define SHAPER_HH
#include "element.hh"

/*
 * =c
 * Shaper(RATE)
 * =s shapes traffic to maximum rate (pkt/s) 
 * V<packet scheduling>
 * =d
 *
 * Shaper is a pull element that allows a maxmimum of RATE packets per second
 * to pass through. That is, output traffic is shaped to RATE packets per
 * second. Shaper is dependent on the timing of its pull requests; if it
 * receives only sporadic pull requests, then it will emit packets only
 * sporadically. However, if it receives a large number of evenly-spaced pull
 * requests, then it will emit packets at the specified RATE with low
 * burstiness.
 *
 * =a BandwidthShaper, RatedSplitter */

class Shaper : public Element { protected:

  unsigned _meter;
  unsigned _ugap;
  unsigned _total;
  struct timeval _start;

 public:
  
  Shaper();
  ~Shaper();
  
  const char *class_name() const                { return "Shaper"; }
  const char *processing() const		{ return PULL; }

  Shaper *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *pull(int);
  
};

#endif
