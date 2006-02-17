// -*- c-basic-offset: 4 -*-
#ifndef CLICK_BWRATEDSPLITTER_HH
#define CLICK_BWRATEDSPLITTER_HH
#include "elements/standard/ratedsplitter.hh"
CLICK_DECLS

/*
 * =c
 * BandwidthRatedSplitter(RATE)
 * =s shaping
 * splits flow of packets at specified bandwidth rate
 * =processing
 * Push
 * =d
 * 
 * BandwidthRatedSplitter has two output ports.  All incoming packets up to a
 * maximum of RATE are emitted on output port 0.  Any remaining packets are
 * emitted on output port 1.  RATE is a bandwidth, such as "384 kbps".
 * Unlike BandwidthMeter, the base RATE is emitted on output port
 * 0 even when the input rate is greater than RATE.
 *
 * =h rate read/write
 * rate of splitting
 *
 * =a RatedSplitter, BandwidthMeter, BandwidthShaper, BandwidthRatedUnqueue */

class BandwidthRatedSplitter : public RatedSplitter { public:
  
    BandwidthRatedSplitter();
    ~BandwidthRatedSplitter();

    const char *class_name() const	{ return "BandwidthRatedSplitter"; }
  
    void push(int port, Packet *);

};

CLICK_ENDDECLS
#endif
