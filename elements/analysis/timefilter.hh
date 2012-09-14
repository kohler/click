// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TIMEFILTER_HH
#define CLICK_TIMEFILTER_HH
#include <click/element.hh>
CLICK_DECLS
class HandlerCall;

/*
=c

TimeFilter(I<keywords START, START_AFTER, START_DELAY, END, END_AFTER, END_DELAY, INTERVAL, STOP, END_CALL>)

=s timestamps

filter packets by timestamp

=d

TimeFilter passes packets through only if they lie within a timestamp range.
You can specify that range absolutely or relative to the first timestamp
TimeFilter sees.

Keyword arguments are:

=over 8

=item START

I<T> is absolute time in seconds since the epoch. Output packets with
timestamps at least I<T>.

=item START_AFTER

I<T> is relative time in seconds (or use suffixes like `ms', `hr'). Output
packets with timestamps at least I<T> seconds after the first packet seen.

=item START_DELAY

I<T> is relative time. Output packets with timestamps at least I<T> seconds
after initialize time.

=item END

I<T> is absolute time. Output packets with timestamps up to I<T>.

=item END_AFTER

I<T> is relative time. Output packets with timestamps up to I<T> seconds after
the first packet seen.

=item END_DELAY

I<T> is relative time. Output packets with timestamps up to I<T> seconds after
initialize time.

=item INTERVAL

I<T> is relative time. Output packets for I<T> seconds after the start time,
as specified by START or START_AFTER.

=item STOP

Boolean. If true, stop the driver once the end time is exceeded. Default is
false.

=item END_CALL

Specifies a write handler to call when the first packet after END is seen.
The variable "$t" in the write handler argument is expanded to the packet's
timestamp.  STOP and END_CALL are mutually exclusive.

=back

Supply at most one of START, START_AFTER, and START_DELAY, and at most one of
END, END_AFTER, END_DELAY, and INTERVAL.

=h start rw

Returns or sets the START time.  If you used START_AFTER or START_DELAY, then
this handler returns garbage until a packet has passed.

=h end rw

Returns or sets the END time.  If you used START_AFTER or START_DELAY, then
this handler returns garbage until a packet has passed.

=h interval rw

Returns or sets the INTERVAL.  Setting INTERVAL leaves the current START as is
and adjusts END.

=h extend_interval w

Takes a timestamp.  Extends the LAST time by that amount.  If extend_interval
is called from an END_CALL handler, then the triggering packet is not dropped.

=a

SetTimestamp */

class TimeFilter : public Element { public:

    TimeFilter() CLICK_COLD;
    ~TimeFilter() CLICK_COLD;

    const char *class_name() const	{ return "TimeFilter"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    Timestamp _first;
    Timestamp _last;

    bool _ready : 1;
    bool _first_relative : 1;
    bool _first_init_relative : 1;
    bool _last_relative : 1;
    bool _last_init_relative : 1;
    bool _last_interval : 1;
    bool _last_h_ready : 1;
    HandlerCall *_last_h;

    void first_packet(const Timestamp &);
    Packet *kill(Packet *);

    enum {
	h_start, h_end, h_interval, h_extend_interval
    };
    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void*, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
