#ifndef BANDWIDTHSHAPER_HH
#define BANDWIDTHSHAPER_HH
#include "element.hh"
#include "timer.hh"
#include "ewma.hh"

/*
 * =c
 * BandwidthShaper(RATE)
 * =s shapes traffic to maximum rate
 * V<packet scheduling>
 * =d
 * BandwidthShaper is a pull element that allows a maximum of RATE
 * bytes per second to pass through. It measures RATE using
 * an exponential weighted moving average.
 *
 * =a Shaper, BandwidthMeter, Meter
 */

class BandwidthShaper : public Element { protected:

  RateEWMA _rate;
  
  unsigned _meter1;

 public:
  
  BandwidthShaper();
  ~BandwidthShaper();
  
  const char *class_name() const                { return "BandwidthShaper"; }
  const char *processing() const		{ return PULL; }

  int rate() const				{ return _rate.average(); }
  int rate_freq() const				{ return _rate.freq(); }
  int rate_scale() const			{ return _rate.scale; }
  
  BandwidthShaper *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  Packet *pull(int);
  
};

#endif
