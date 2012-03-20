#ifndef CLICK_TIMEDSOURCE_HH
#define CLICK_TIMEDSOURCE_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

TimedSource([INTERVAL, DATA, I<KEYWORDS>])

=s basicsources

periodically generates a packet

=d

Creates packets consisting of DATA. Pushes such a packet out its single output
about once every INTERVAL seconds. Default
INTERVAL is 500 milliseconds; default DATA is at least 64 bytes long.

Keyword arguments are:

=over 8

=item DATA

String. Same as the DATA argument.

=item INTERVAL

Time. Same as the INTERVAL argument.

=item LIMIT

Integer. Stops sending after LIMIT packets are generated; but if LIMIT is
negative, sends packets forever.

=item HEADROOM

Unsigned integer. Sets the amount of headroom on generated packets. Default is
the default packet headroom.

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

  const char *class_name() const		{ return "TimedSource"; }
  const char *port_count() const		{ return PORTS_0_1; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();

  void run_timer(Timer *);

 private:

    Packet *_packet;
    Timestamp _interval;
    int _limit;
    int _count;
    bool _active;
    bool _stop;
    Timer _timer;
    String _data;
    uint32_t _headroom;

    enum { h_data, h_interval, h_active, h_reset, h_headroom };
    static String read_param(Element *, void *);
    static int change_param(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
