#ifndef BWRATEDSPLITTER_HH
#define BWRATEDSPLITTER_HH
#include "elements/standard/ratedsplitter.hh"

/*
 * =c
 * BandwidthRatedSplitter(R)
 * =s
 * splits flow of packets at specified bandwidth rate
 * V<classification>
 * =processing
 * Push
 * =d
 * 
 * BandwidthRatedSplitter has two output ports. All incoming packets up to a
 * maximum of R bytes per second are emitted on output port 0. Any remaining
 * packets are emitted on output port 1. Unlike BandwidthMeter, R packets per
 * second are emitted on output port 0 even when the input rate is greater
 * than R.
 *
 * =h rate read/write
 * rate of splitting
 *
 * =a RatedSplitter, BandwidthMeter, BandwidthShaper, BandwidthRatedUnqueue */

class BandwidthRatedSplitter : public RatedSplitter {

 public:
  
  BandwidthRatedSplitter()		{ }

  const char *class_name() const	{ return "BandwidthRatedSplitter"; }
  BandwidthRatedSplitter *clone() const	{ return new BandwidthRatedSplitter; }
  
  void push(int port, Packet *);

};

#endif
