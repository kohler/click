#ifndef REVERSE_SORTED_SCHED_HH
#define REVERSE_SORTED_SCHED_HH

/*
 * =c
 * ReverseSortedTaskSched([INTERVAL])
 * =s IP
 * bin packing scheduler
 * =d
 *
 * Bin pack tasks onto threads by minimizing variance in load.  INTERVAL
 * specifies the number of ms between each load balance. By default it is 1000
 * (1 second).
 */

#include <click/element.hh>
#include <click/timer.hh>

class ReverseSortedTaskSched : public Element {

  Timer _timer;
  int _interval;

 public:

  ReverseSortedTaskSched();
  ~ReverseSortedTaskSched();
  
  const char *class_name() const	{ return "ReverseSortedTaskSched"; }
  int configure(const Vector<String> &, ErrorHandler *);
  ReverseSortedTaskSched *clone() const	{ return new ReverseSortedTaskSched; }

  int initialize(ErrorHandler *);
  void run_scheduled();
};

#endif


