#ifndef CLICK_TIMEDSINK_HH
#define CLICK_TIMEDSINK_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
 * =c
 * TimedSink(I)
 * =s sinks
 * periodically pulls and drops a packet
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
  ~TimedSink();
  
  const char *class_name() const		{ return "TimedSink"; }
  const char *processing() const		{ return PULL; }
  
  TimedSink *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void run_scheduled();
  
};

CLICK_ENDDECLS
#endif
