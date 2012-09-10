#ifndef CLICK_AGGREGATELAST_HH
#define CLICK_AGGREGATELAST_HH
#include <click/element.hh>
#include <click/task.hh>
#include "aggregatenotifier.hh"
CLICK_DECLS

/*
=c

AggregateLast([I<KEYWORDS>])

=s aggregates

lets through last packet per aggregate annotation

=d

AggregateLast forwards only the final packet with a given aggregate annotation
value. All previous packets with that aggregate annotation are emitted on the
second output, if it exists, or dropped if it does not.

The output packet will have EXTRA_PACKETS_ANNO, EXTRA_LENGTH_ANNO, and
FIRST_TIMESTAMP_ANNO set to the values reflecting the total volume of the
aggregate.

Keyword arguments are:

=over 8

=item NOTIFIER

The name of an AggregateNotifier element, like AggregateIPFlows. If given,
then AggregateLast will output a packet when the AggregateNotifier informs it
that the packet's aggregate is complete. This can save significant memory on
long traces.

=item STOP_AFTER_CLEAR

Boolean. If true, then stop the router after the 'clear' handler is called and
completes. Default is false.

=back

=h clear write-only

When written, AggregateLast will output every packet it has stored and clear
its tables. This is the only time AggregateLast will emit packets if NOTIFIER
was not set.

=n

AggregateFirst forwards the the first packet with a given aggregate annotation
value, rather than the last packet. It has significantly smaller memory
requirements than AggregateLast.

Only available in user-level processes.

=a

AggregateFirst, AggregateIP, AggregateIPFlows, AggregateCounter,
AggregateFilter */

class AggregateLast : public Element, public AggregateListener { public:

    AggregateLast() CLICK_COLD;
    ~AggregateLast() CLICK_COLD;

    const char *class_name() const	{ return "AggregateLast"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    void push(int, Packet *);
    bool run_task(Task *);

    void aggregate_notify(uint32_t, AggregateEvent, const Packet *);
    void add_handlers() CLICK_COLD;

  private:

    enum { ROW_BITS = 10, ROW_SHIFT = 0, NROW = 1<<ROW_BITS, ROW_MASK = NROW - 1,
	   COL_BITS = 16, COL_SHIFT = ROW_BITS, NCOL = 1<<COL_BITS, COL_MASK = NCOL - 1,
	   PLANE_BITS = 32 - ROW_BITS - COL_BITS, PLANE_SHIFT = COL_SHIFT + COL_BITS, NPLANE = 1<<PLANE_BITS, PLANE_MASK = NPLANE - 1 };

    Packet ***_packets[NPLANE];
    AggregateNotifier *_agg_notifier;
    uint32_t *_counts[NPLANE];

    Task _clear_task;
    uint32_t _needs_clear;	// XXX atomic
    bool _stop_after_clear;

    Packet **create_row(uint32_t agg);
    inline Packet **row(uint32_t agg);
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

inline Packet **
AggregateLast::row(uint32_t agg)
{
    if (Packet ***p = _packets[(agg >> PLANE_SHIFT) & PLANE_MASK])
	if (Packet **c = p[(agg >> COL_SHIFT) & COL_MASK])
	    return c;
    return create_row(agg);
}

CLICK_ENDDECLS
#endif
