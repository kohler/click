#ifndef TIMEDSINK_HH
#define TIMEDSINK_HH
#include "element.hh"
#include "timer.hh"

/*
 * =c
 * TimedSink(I)
 * =d
 * Pulls one packet every I seconds from its input.
 * Discards the packet.
 * =a Shaper
 */

class TimedSink : public Element {

  Timer _timer;
  int _interval;
  
 public:
  
  TimedSink();
  
  const char *class_name() const		{ return "TimedSink"; }
  Processing default_processing() const	{ return PULL; }
  
  TimedSink *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();
  
};

#endif
