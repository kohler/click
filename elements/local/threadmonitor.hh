#ifndef THREADMONITOR_HH
#define THREADMONITOR_HH

/*
 * =c
 * ThreadMonitor([INTERVAL, THRESH])
 * =s IP
 * print out thread status
 * =d
 *
 * Every INTERVAL number of ms, print out tasks scheduled on each thread if
 * tasks are busy. INTERVAL by default is 1000 ms. Only tasks with cycle count
 * above THRESH are printed. By default THRESH is 1000 cycles.
 */

#ifdef __MTCLICK__

#include <click/element.hh>
#include <click/timer.hh>

class ThreadMonitor : public Element {

  Timer _timer;
  unsigned _interval;
  unsigned _thresh;

 public:

  ThreadMonitor();
  ~ThreadMonitor();
  
  const char *class_name() const	{ return "ThreadMonitor"; }
  ThreadMonitor *clone() const	        { return new ThreadMonitor; }
  int configure(const Vector<String> &, ErrorHandler *);

  int initialize(ErrorHandler *);
  void run_scheduled();
};

#endif

#endif
