// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SORTEDSCHED_HH
#define CLICK_SORTEDSCHED_HH

/*
 * =c
 * BalancedThreadSched([INTERVAL, INCREASING])
 * =s threads
 * bin packing scheduler
 * =d
 *
 * Bin pack tasks onto threads by minimizing variance in load. INTERVAL
 * specifies the number of ms between each load balance. By default it is 1000
 * (1 second). If INCREASING is true, first sort tasks in increasing
 * order based on cost, then binpack. Otherwise, tasks are decreasingly
 * sorted. By default, INCREASING is true.
 *
 * =a ThreadMonitor, StaticThreadSched
 */

#include <click/element.hh>
#include <click/timer.hh>

class BalancedThreadSched : public Element { public:

    BalancedThreadSched() CLICK_COLD;
    ~BalancedThreadSched() CLICK_COLD;

    const char *class_name() const	{ return "BalancedThreadSched"; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    int initialize(ErrorHandler *) CLICK_COLD;
    void run_timer(Timer *);

  private:

    Timer _timer;
    int _interval;
    bool _increasing;

};

#endif


