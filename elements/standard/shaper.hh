// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_SHAPER_HH
#define CLICK_SHAPER_HH
#include <click/element.hh>
#include <click/gaprate.hh>
CLICK_DECLS

/*
 * =c
 * Shaper(RATE)
 * =s shaping
 * shapes traffic to maximum rate (pkt/s)
 * =processing
 * Push
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
 * Pull requests on optional output port 1 return packets that do not meet the
 * shaping condition at the time of the pull. Note that such pull requests
 * may pull packets from upstream that would otherwise have been emitted
 * later on port 0. (This is different from L<RatedSplitter>, whose optional
 * output port 1 emits packets that would otherwise have been dropped.)
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
 * =h rate read/write
 *
 * Returns or sets the RATE parameter.
 *
 * =a BandwidthShaper, RatedSplitter, RatedUnqueue */

class Shaper : public Element { public:

    Shaper() CLICK_COLD;

    const char *class_name() const	{ return "Shaper"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PULL; }
    bool is_bandwidth() const		{ return class_name()[0] == 'B'; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *pull(int);

  protected:

    GapRate _rate;

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *e, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
