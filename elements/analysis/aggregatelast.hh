#ifndef CLICK_AGGREGATELAST_HH
#define CLICK_AGGREGATELAST_HH
#include <click/element.hh>
#include <click/task.hh>
#include "aggregatenotifier.hh"
CLICK_DECLS

/*
=c

AggregateLast([I<KEYWORDS>])

=s measurement

lets through first packet per aggregate annotation

=d

AggregateLast forwards only the first packet with a given aggregate
annotation value. Second and subsequent packets with that aggregate annotation
are emitted on the second output, if it exists, or dropped if it does not.

Keyword arguments are:

=over 8

=item NOTIFIER

The name of an AggregateNotifier element, like AggregateIPFlows. If given,
then AggregateLast will prune information about old aggregates. This can save
significant memory on long traces.

=back

=n

AggregateLast forwards the last packet with a given aggregate annotation
value, and additionally annotates the packet with the observed packet and byte
counts. AggregateLast has significantly lower memory requirements, however.

Only available in user-level processes.

=a

AggregateLast, AggregateIP, AggregateIPFlows, AggregateCounter, AggregateFilter */

class AggregateLast : public Element, public AggregateListener { public:
  
    AggregateLast();
    ~AggregateLast();
  
    const char *class_name() const	{ return "AggregateLast"; }
    const char *processing() const	{ return PUSH; }
    AggregateLast *clone() const	{ return new AggregateLast; }

    void notify_noutputs(int);
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    void push(int, Packet *);
    void run_scheduled();

    void aggregate_notify(uint32_t, AggregateEvent, const Packet *);
    void add_handlers();
    
  private:

    enum { ROW_BITS = 10, ROW_SHIFT = 0, NROW = 1<<ROW_BITS, ROW_MASK = NROW - 1,
	   COL_BITS = 16, COL_SHIFT = ROW_BITS, NCOL = 1<<COL_BITS, COL_MASK = NCOL - 1,
	   PLANE_BITS = 32 - ROW_BITS - COL_BITS, PLANE_SHIFT = COL_SHIFT + COL_BITS, NPLANE = 1<<PLANE_BITS, PLANE_MASK = NPLANE - 1 };
    
    Packet ***_packets[NPLANE];
    AggregateNotifier *_agg_notifier;
    uint32_t *_counts[NPLANE];
    
    Task _clean_task;
    uint32_t _needs_clean;	// XXX atomic

    Packet **create_row(uint32_t agg);
    inline Packet **row(uint32_t agg);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

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
