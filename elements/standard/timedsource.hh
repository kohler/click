#ifndef TIMEDSOURCE_HH
#define TIMEDSOURCE_HH
#include <click/element.hh>
#include <click/timer.hh>

/*
 * =c
 * TimedSource(I)
 * TimedSource(I, DATA)
 * =s
 * periodically generates a packet
 * V<sources>
 * =d
 * Creates packets consisting of DATA. Pushes such a packet out
 * its single output about once every I seconds.
 * Default DATA is at least 64 bytes long.
 * =n
 * Linux kernel timers have low granularity, so this element won't perform
 * very well in the kernel.
 * =a InfiniteSource
 */

class TimedSource : public Element {
  
  String _data;
  Timer _timer;
  int _interval;
  
 public:
  
  TimedSource();
  ~TimedSource();
  
  const char *class_name() const		{ return "TimedSource"; }
  const char *processing() const		{ return PUSH; }
  
  TimedSource *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();
  
};

#endif
