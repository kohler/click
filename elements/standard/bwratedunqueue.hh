#ifndef CLICK_BWRATEDUNQUEUE_HH
#define CLICK_BWRATEDUNQUEUE_HH
#include "elements/standard/ratedunqueue.hh"

/*
 * =c
 * BandwidthRatedUnqueue(RATE)
 * =s packet scheduling
 * pull-to-push converter
 * =processing
 * Pull inputs, push outputs
 * =d
 * 
 * Pulls packets at the given bandwidth RATE (that is, bytes per second), and
 * pushes them out its single output.
 *
 * =a RatedUnqueue, Unqueue, BandwidthShaper, BandwidthRatedSplitter */

class BandwidthRatedUnqueue : public RatedUnqueue { public:
  
  BandwidthRatedUnqueue();
  ~BandwidthRatedUnqueue();
  
  const char *class_name() const	{ return "BandwidthRatedUnqueue"; }
  BandwidthRatedUnqueue *clone() const	{ return new BandwidthRatedUnqueue; }
  
  void run_scheduled();
  
};

#endif
