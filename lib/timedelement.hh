#ifndef TIMEDELEMENT_HH
#define TIMEDELEMENT_HH
#include "element.hh"
#include "timer.hh"

class TimedElement : public Element {
  
  Timer _timer;
  int _interval_ms;
  
 public:
  
  TimedElement();
  ~TimedElement();
  
  bool timer_scheduled() const	{ return _timer.scheduled(); }
  void timer_schedule_next()	{ _timer.schedule_after_ms(_interval_ms); }
  void timer_schedule_after_ms(int ms) { _timer.schedule_after_ms(ms); }
  void timer_unschedule()	{ _timer.unschedule(); }
  void set_interval_ms(int ms)  { _interval_ms = ms; }
  
  int configure(const String &, ErrorHandler *);
  void uninitialize();
  
};

#endif
