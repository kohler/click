#ifndef CLICK_TIMEDSOURCE_HH
#define CLICK_TIMEDSOURCE_HH
#include <click/element.hh>
#include <click/timer.hh>

/*
=c

TimedSource([INTERVAL, DATA, I<KEYWORDS>])

=s sources

periodically generates a packet

=d

Creates packets consisting of DATA. Pushes such a packet out its single output
about once every INTERVAL seconds. INTERVAL has millisecond precision. Default
INTERVAL is 500 milliseconds; default DATA is at least 64 bytes long.

Keyword arguments are:

=over 8

=item DATA

String. Same as the DATA argument.

=item INTERVAL

Number of seconds. Same as the INTERVAL argument.

=item LIMIT

Integer. Stops sending after LIMIT packets are generated; but if LIMIT is
negative, sends packets forever.

=item STOP

Boolean. If true, then stop the driver once LIMIT packets are sent. Default is
false.

=back

=e

  TimedSource(INTERVAL 0.333) -> ...

=h data read/write

Returns or sets the DATA parameter.

=h interval read/write

Returns or sets the INTERVAL parameter.

=a InfiniteSource */

class TimedSource : public Element { public:
  
  TimedSource();
  ~TimedSource();
  
  const char *class_name() const		{ return "TimedSource"; }
  const char *processing() const		{ return PUSH; }
  
  TimedSource *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  void run_scheduled();
  
 private:
  
  Packet *_packet;
  uint32_t _interval;
  int _limit;
  int _count;
  bool _active : 1;
  bool _stop : 1;
  Timer _timer;
  String _data;

  static String read_param(Element *, void *);
  static int change_param(const String &, Element *, void *, ErrorHandler *);
  
};

#endif
