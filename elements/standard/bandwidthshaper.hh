// -*- c-basic-offset: 4 -*-
#ifndef CLICK_BANDWIDTHSHAPER_HH
#define CLICK_BANDWIDTHSHAPER_HH
#include "shaper.hh"
CLICK_DECLS

/*
 * =c
 * BandwidthShaper(RATE)
 * =s shaping
 * shapes traffic to maximum rate (bytes/s)
 * =processing
 * Pull
 * =d
 *
 * BandwidthShaper is a pull element that allows a maximum bandwidth of
 * RATE to pass through.  That is, output traffic is shaped to RATE.
 * If a BandwidthShaper receives a large number of
 * evenly-spaced pull requests, then it will emit packets at the specified
 * RATE with low burstiness.
 *
 * =h rate read/write
 *
 * Returns or sets the RATE parameter.
 *
 * =a Shaper, BandwidthRatedSplitter, BandwidthRatedUnqueue */

class BandwidthShaper : public Shaper { public:

    BandwidthShaper() CLICK_COLD;

    const char *class_name() const	{ return "BandwidthShaper"; }

    Packet *pull(int);

};

CLICK_ENDDECLS
#endif
