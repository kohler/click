#ifndef SHAPER_HH
#define SHAPER_HH
#include "element.hh"
#include "gaprate.hh"

/*
 * =c
 * Shaper(RATE)
 * =s shapes traffic to maximum rate (pkt/s) 
 * V<packet scheduling>
 * =d
 *
 * Shaper is a pull element that allows a maximum of RATE packets per second
 * to pass through. That is, traffic is shaped to RATE packets per
 * second. Shaper is dependent on the timing of its pull requests; if it
 * receives only sporadic pull requests, then it will emit packets only
 * sporadically. However, if it receives a large number of evenly-spaced pull
 * requests, then it will emit packets at the specified RATE with low
 * burstiness.
 *
 * =n
 *
 * Shaper cannot implement every rate smoothly. For example, it can smoothly
 * generate 1000000 packets per second and 1000244 packets per second, but not
 * rates in between. (In-between rates will result in minor burstiness.) This
 * granularity issue is negligible at low rates, and becomes serious at very
 * high rates; for example, Shaper cannot smoothly implement any rate between
 * 2.048e10 and 4.096e10 packets per second.
 *
 * =a BandwidthShaper, RatedSplitter, RatedUnqueue */

class Shaper : public Element { protected:

  GapRate _rate;

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
