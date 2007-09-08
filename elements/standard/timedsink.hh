#ifndef CLICK_TIMEDSINK_HH
#define CLICK_TIMEDSINK_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
 * =c
 * TimedSink([INTERVAL])
 * =s basicsources
 * periodically pulls and drops a packet
 * =d
 * Pulls one packet every INTERVAL seconds from its input.
 * Discards the packet.  Default INTERVAL is 500 milliseconds.
 * =a Shaper
 */

class TimedSink : public Element {

  Timer _timer;
  int _interval;
  
 public:
  
  TimedSink();
  ~TimedSink();
  
  const char *class_name() const		{ return "TimedSink"; }
  const char *port_count() const		{ return PORTS_1_0; }
  const char *processing() const		{ return PULL; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void run_timer(Timer *);
  
};

CLICK_ENDDECLS
#endif
