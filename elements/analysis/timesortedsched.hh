// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TIMESORTEDSCHED_HH
#define CLICK_TIMESORTEDSCHED_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

TimeSortedSched(I<keywords> STOP, BUFFER)

=s timestamps

merge sorted packet streams by timestamp

=io

one output, zero or more inputs

=d

TimeSortedSched responds to pull requests by returning the chronologically
next packet pulled from its inputs, determined by packet timestamps.

TimeSortedSched expects its input packet streams to arrive sorted by
timestamp.  If the C<well_ordered> handler returns "false", then one or more
packet streams did not arrive correctly sorted by timestamp, so
TimeSortedSched emitted some packets out of order.  (But see BUFFER, below.)

TimeSortedSched listens for notification from its inputs to avoid useless
pulls, and provides notification for its output.

Keyword arguments are:

=over 8

=item STOP

Boolean. If true, stop the driver when there are no packets available (and the
upstream notifiers indicate that no packets will become available soon).
Default is false.

=item BUFFER

Integer. Up to BUFFER packets per input are buffered within
TimeSortedSched. Default BUFFER is 1. Higher BUFFER values let TimeSortedSched
cope with minor reordering in its input streams.

=back

=n

TimeSortedSched is a notifier signal, active iff any of the upstream notifiers
are active.

=e

This example merges multiple tcpdump(1) files into a single, time-sorted
stream, and stops the driver when all the files are exhausted.

  tss :: TimeSortedSched(STOP true);
  FromDump(FILE1) -> [0] tss;
  FromDump(FILE2) -> [1] tss;
  FromDump(FILE3) -> [2] tss;
  // ...
  tss -> ...;

=h well_ordered r

Returns a Boolean string. If "false", then TimeSortedSched's output was not
properly sorted by increasing timestamp, because one or more of its input
streams was not so sorted.

=a

FromDump
*/

class TimeSortedSched : public Element { public:

    TimeSortedSched() CLICK_COLD;
    ~TimeSortedSched() CLICK_COLD;

    const char *class_name() const	{ return "TimeSortedSched"; }
    const char *port_count() const	{ return "-/1"; }
    const char *processing() const	{ return PULL; }
    const char *flags() const		{ return "S0"; }
    void *cast(const char *);

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void cleanup(CleanupStage stage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *pull(int);

  private:

    struct packet_s {
	Packet *p;
	int input;		// for space, consider using annotation?
    };
    struct heap_less {
	inline bool operator()(packet_s &a, packet_s &b) {
	    return a.p->timestamp_anno() < b.p->timestamp_anno();
	}
    };
    struct input_s {
	NotifierSignal signal;
	int space;
	int ready;
    };

    packet_s *_pkt;
    int _npkt;

    input_s *_input;
    int _nready;

    Notifier _notifier;
    int _buffer;
    Timestamp _last_emission;
    bool _stop;
    bool _well_ordered;

};

CLICK_ENDDECLS
#endif
