#ifndef OLDSHAPER_HH
#define OLDSHAPER_HH
#include "bandwidthshaper.hh"

/*
 * =c
 * Shaper(RATE)
 * =s shapes traffic to maximum rate (pkt/s)
 * V<packet scheduling>
 * =d
 * Shaper is a pull element that allows a maximum of RATE packets per
 * second to pass through. It measures RATE using an exponential weighted
 * moving average. Because the granularity of the timer used for calculating
 * the moving average is coarse, Shaper is not accurate for rates less
 * than 30000. Use SlowShaper instead for those rates.
 *
 * There are usually Queues both upstream and downstream
 * of Shaper elements.
 * =a SlowShaper, BandwidthShaper, Meter, BandwidthMeter
 */

class Shaper : public BandwidthShaper {

 public:
  
  Shaper();
  
  const char *class_name() const                { return "Shaper"; }
  Shaper *clone() const;
  
  Packet *pull(int);
  
};

#endif
