#ifndef CLICK_INFINITESOURCE_HH
#define CLICK_INFINITESOURCE_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS
class HandlerCall;

/*
=c

InfiniteSource([DATA, LIMIT, BURST, ACTIVE, I<keywords>])

=s basicsources

generates packets whenever scheduled

=d

Creates packets consisting of DATA. Pushes BURST such packets out its single
output every time it is scheduled (which will be often). Stops sending after
LIMIT packets are generated; but if LIMIT is negative, sends packets forever.
Will send packets only if ACTIVE is true. (ACTIVE is true by default.) Default
DATA is at least 64 bytes long. Default LIMIT is -1 (send packets forever).
Default BURST is 1.

Keyword arguments are:

=over 8

=item DATA

String. Same as the DATA argument.

=item LENGTH

Integer. If set, the outgoing packet will have this length.

=item LIMIT

Integer. Same as the LIMIT argument.

=item BURST

Integer. Same as the BURST argument.

=item ACTIVE

Boolean. Same as the ACTIVE argument.

=item STOP

Boolean. If true, then stop the driver once LIMIT packets are sent. Default is
false.

=item END_CALL

A write handler called once LIMIT packets are sent. END_CALL and
STOP are mutually exclusive.

=item TIMESTAMP

Boolean. If false, do not set the timestamp annotation on generated
packets. Defaults to true.

=back

To generate a particular traffic pattern, use this element and RatedSource
in conjunction with Script.

=e

  InfiniteSource(\<0800>) -> Queue -> ...

=n

Useful for profiling and experiments.  Packets' timestamp annotations are set
to the current time.

InfiniteSource listens for downstream full notification.

=h count read-only
Returns the total number of packets that have been generated.
=h reset write-only
Resets the number of generated packets to 0. The InfiniteSource will then
generate another LIMIT packets (if it is active).
=h data read/write
Returns or sets the DATA parameter.
=h length read/write
Returns or sets the LENGTH parameter.
=h limit read/write
Returns or sets the LIMIT parameter.
=h burst read/write
Returns or sets the BURST parameter.
=h active read/write
Makes the element active or inactive.

=a

RatedSource, Script */

class InfiniteSource : public Element, public ActiveNotifier { public:

  InfiniteSource();

  const char *class_name() const		{ return "InfiniteSource"; }
  void *cast(const char *);
  const char *port_count() const		{ return PORTS_0_1; }
  const char *flags() const			{ return "S1"; }
  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  void cleanup(CleanupStage);

  bool run_task(Task *);
  Packet *pull(int);

  protected:

#if HAVE_INT64_TYPES
    typedef uint64_t ucounter_t;
    typedef int64_t counter_t;
#else
    typedef uint32_t ucounter_t;
    typedef int32_t counter_t;
#endif

    void setup_packet();

    Packet *_packet;
    int _burstsize;
    counter_t _limit;
    ucounter_t _count;
    int _datasize;
    bool _active;
    bool _timestamp;
    Task _task;
    String _data;
    NotifierSignal _nonfull_signal;
    HandlerCall *_end_h;

    enum {
	h_data, h_limit, h_burst, h_active, h_reset, h_count, h_length,
	h_notifier
    };
    static int change_param(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
