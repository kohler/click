#ifndef BANDWIDTHSHAPER_HH
#define BANDWIDTHSHAPER_HH
#include "shaper.hh"

/*
 * =c
 * BandwidthShaper(RATE)
 * =s shapes traffic to maximum rate (bytes/s) 
 * V<packet scheduling>
 * =processing
 * Pull
 * =d
 *
 * BandwidthShaper is a pull element that allows a maximum of RATE bytes per
 * second to pass through. That is, output traffic is shaped to RATE bytes per
 * second. If a BandwidthShaper receives a large number of evenly-spaced pull
 * requests, then it will emit packets at the specified RATE with low
 * burstiness.
 *
 * =a Shaper, BandwidthRatedSplitter, BandwidthRatedUnqueue */

class BandwidthShaper : public Shaper {

 public:
  
  BandwidthShaper()				{ }
  
  const char *class_name() const                { return "BandwidthShaper"; }
  BandwidthShaper *clone() const;

  Packet *pull(int);
  
};

#endif
