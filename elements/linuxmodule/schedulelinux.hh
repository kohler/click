#ifndef SCHEDULELINUX_HH
#define SCHEDULELINUX_HH
#include "element.hh"

/*
 * =c
 * ScheduleLinux()
 * =d
 * Go back to linux scheduler.
 */

class ScheduleLinux : public Element {
  
 public:
  
  ScheduleLinux()  {}
  ~ScheduleLinux() {}
  
  const char *class_name() const		{ return "ScheduleLinux"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  ScheduleLinux *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  
};

#endif
