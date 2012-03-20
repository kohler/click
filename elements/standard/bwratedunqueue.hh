// -*- c-basic-offset: 4 -*-
#ifndef CLICK_BWRATEDUNQUEUE_HH
#define CLICK_BWRATEDUNQUEUE_HH
#include "elements/standard/ratedunqueue.hh"
CLICK_DECLS

/*
 * =c
 * BandwidthRatedUnqueue(RATE, I[<KEYWORDS>])
 * =s shaping
 * pull-to-push converter
 * =d
 *
 * Pulls packets at the given RATE, and pushes them out its single output.  This
 * rate is implemented using a token bucket.  The capacity of this token bucket
 * defaults to 20 milliseconds worth of tokens, but can be customized by setting
 * one of BURST_DURATION or BURST_SIZE.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item RATE
 *
 * Bandwidth.  Token bucket fill rate.
 *
 * =item BURST_DURATION
 *
 * Time.  If specified, the capacity of the token bucket is calculated as
 * rate * burst_duration.
 *
 * =item BURST_BYTES
 *
 * Integer.  If specified, the capacity of the token bucket is set to this
 * value in bytes.
 *
 * =h rate read/write
 *
 * =a RatedUnqueue, Unqueue, BandwidthShaper, BandwidthRatedSplitter */

class BandwidthRatedUnqueue : public RatedUnqueue { public:

    BandwidthRatedUnqueue();

    const char *class_name() const	{ return "BandwidthRatedUnqueue"; }

    bool run_task(Task *);

};

CLICK_ENDDECLS
#endif
