// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RATEDUNQUEUE_HH
#define CLICK_RATEDUNQUEUE_HH
#include <click/element.hh>
#include <click/gaprate.hh>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * RatedUnqueue(RATE)
 * =s shaping
 * pull-to-push converter
 * =d
 *
 * Pulls packets at the given RATE in packets per second, and pushes them out
 * its single output.
 *
 * RatedUnqueue will use a lot of CPU if given a low RATE.  This is because it
 * maintains the RATE by constantly rescheduling itself until it's time for a
 * packet to be emitted.  TimedUnqueue is often a better choice for low RATEs.
 *
 * =h rate read/write
 *
 * =a BandwidthRatedUnqueue, Unqueue, TimedUnqueue, Shaper, RatedSplitter */

class RatedUnqueue : public Element { public:

    RatedUnqueue();
    ~RatedUnqueue();

    const char *class_name() const	{ return "RatedUnqueue"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return PULL_TO_PUSH; }
    bool is_bandwidth() const		{ return class_name()[0] == 'B'; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const	{ return true; }
    int initialize(ErrorHandler *);
    void add_handlers();

    bool run_task(Task *);

  protected:

    GapRate _rate;
    Task _task;
    enum { use_signal = 1 };
    NotifierSignal _signal;

    static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
