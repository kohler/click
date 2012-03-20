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
 * =h interval read/write
 * Returns or sets the INTERVAL parameter.
 * =a Shaper
 */

class TimedSink : public Element { public:

  TimedSink();

  const char *class_name() const		{ return "TimedSink"; }
  const char *port_count() const		{ return PORTS_1_0; }
  const char *processing() const		{ return PULL; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  void run_timer(Timer *);

  private:

    Timer _timer;
    Timestamp _interval;

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
