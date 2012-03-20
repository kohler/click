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

    Shaper();

    const char *class_name() const	{ return "Shaper"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return PULL; }
    bool is_bandwidth() const		{ return class_name()[0] == 'B'; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers();

    Packet *pull(int);

  protected:

    GapRate _rate;

    static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
