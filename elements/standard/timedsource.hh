#ifndef TIMEDSOURCE_HH
#define TIMEDSOURCE_HH
#include "element.hh"
#include "timer.hh"

class TimedSource : public Element {
  
  String _data;
  Timer _timer;
  int _interval;
  
 public:
  
  TimedSource();
  
  const char *class_name() const		{ return "TimedSource"; }
  Processing default_processing() const	{ return PUSH; }
  
  TimedSource *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  int initialize(Router *, ErrorHandler *);
  void uninitialize(Router *);
  
  void run_scheduled();
  
};

#endif
