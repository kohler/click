#ifndef TIMEDSINK_HH
#define TIMEDSINK_HH
#include "element.hh"
#include "timer.hh"

/*
 * =c
 * TimedSink(I)
 * =s
 * periodically pulls and drops a packet
 * V<sinks>
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
  const char *processing() const	{ return PULL; }
  
  TimedSink *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();
  
};

#endif
