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
 * =s packet scheduling
 * pull-to-push converter
 * =d
 * 
 * Pulls packets at the given RATE in packets per second, and pushes them out
 * its single output.
 *
 * =h rate read/write
 *
 * =a BandwidthRatedUnqueue, Unqueue, Shaper, RatedSplitter */

class RatedUnqueue : public Element { public:
  
    RatedUnqueue();
    ~RatedUnqueue();
  
    const char *class_name() const	{ return "RatedUnqueue"; }
    const char *processing() const	{ return PULL_TO_PUSH; }
    bool is_bandwidth() const		{ return class_name()[0] == 'B'; }
  
    int configure(Vector<String> &, ErrorHandler *);
    void configuration(Vector<String> &) const;
    int initialize(ErrorHandler *);
    void add_handlers();
  
    bool run_task();

  protected:

    GapRate _rate;
    Task _task;
    NotifierSignal _signal;
  
};

CLICK_ENDDECLS
#endif
