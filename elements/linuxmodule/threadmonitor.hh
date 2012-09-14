// -*- c-basic-offset: 4 -*-
#ifndef CLICK_THREADMONITOR_HH
#define CLICK_THREADMONITOR_HH

/*
 * =c
 * ThreadMonitor([INTERVAL, THRESH])
 * =s threads
 * print out thread status
 * =d
 *
 * Every INTERVAL number of ms, print out tasks scheduled on each thread if
 * tasks are busy. INTERVAL by default is 1000 ms. Only tasks with cycle count
 * of at least THRESH are printed. By default THRESH is 1000 cycles.
 *
 * =a
 * StaticThreadSched, BalancedThreadSched
 */

#include <click/element.hh>
#include <click/timer.hh>

class ThreadMonitor : public Element { public:

    ThreadMonitor() CLICK_COLD;
    ~ThreadMonitor() CLICK_COLD;

    const char *class_name() const	{ return "ThreadMonitor"; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    int initialize(ErrorHandler *) CLICK_COLD;
    void run_timer(Timer *);

  private:

    Timer _timer;
    unsigned _interval;
    unsigned _thresh;

};

#endif

