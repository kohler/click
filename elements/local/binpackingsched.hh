#ifndef BINPACKING_SCHED_HH
#define BINPACKING_SCHED_HH

/*
 * =c
 * BinPackingScheduler([INTERVAL])
 * =s IP
 * bin packing scheduler
 * =d
 *
 * Use a minimizing largest bin type of bin packing algorithm to load balance
 * elements onto threads. INTERVAL specifies the number of ms between each
 * load balance. By default it is 1000 (1 second).
 */

#include <click/element.hh>
#include <click/timer.hh>

class BinPackingScheduler : public Element {

  Timer _timer;
  int _interval;

 public:

  BinPackingScheduler();
  ~BinPackingScheduler();
  
  const char *class_name() const	{ return "BinPackingScheduler"; }
  int configure(const Vector<String> &, ErrorHandler *);
  BinPackingScheduler *clone() const	{ return new BinPackingScheduler; }

  int initialize(ErrorHandler *);
  void run_scheduled();
};

#endif


