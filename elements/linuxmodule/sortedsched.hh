// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SORTEDSCHED_HH
#define CLICK_SORTEDSCHED_HH

/*
 * =c
 * BalanceThreadSched([INTERVAL, INCREASING])
 * =s IP
 * bin packing scheduler
 * =d
 *
 * Bin pack tasks onto threads by minimizing variance in load. INTERVAL
 * specifies the number of ms between each load balance. By default it is 1000
 * (1 second). If increasing is specified, first sort tasks in increasing
 * order based on cost, then binpack. Otherwise, tasks are decreasingly
 * sorted. By default, INCREASING is true.
 *
 * =a ThreadMonitor, StaticThreadSched
 */

#include <click/element.hh>
#include <click/timer.hh>

class BalanceThreadSched : public Element { public:

    BalanceThreadSched();
    ~BalanceThreadSched();
  
    const char *class_name() const	{ return "BalanceThreadSched"; }
    int configure(Vector<String> &, ErrorHandler *);

    int initialize(ErrorHandler *);
    void run_timer();

  private:

    Timer _timer;
    int _interval;
    bool _increasing;

};

#endif


