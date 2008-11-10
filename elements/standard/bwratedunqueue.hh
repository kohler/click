// -*- c-basic-offset: 4 -*-
#ifndef CLICK_BWRATEDUNQUEUE_HH
#define CLICK_BWRATEDUNQUEUE_HH
#include "elements/standard/ratedunqueue.hh"
CLICK_DECLS

/*
 * =c
 * BandwidthRatedUnqueue(RATE)
 * =s shaping
 * pull-to-push converter
 * =processing
 * Pull inputs, push outputs
 * =d
 *
 * Pulls packets at the given bandwidth RATE, and pushes them out its single
 * output.  RATE is a bandwidth, such as "384 kbps".
 *
 * =a RatedUnqueue, Unqueue, BandwidthShaper, BandwidthRatedSplitter */

class BandwidthRatedUnqueue : public RatedUnqueue { public:

    BandwidthRatedUnqueue();
    ~BandwidthRatedUnqueue();

    const char *class_name() const	{ return "BandwidthRatedUnqueue"; }

    bool run_task(Task *);

};

CLICK_ENDDECLS
#endif
