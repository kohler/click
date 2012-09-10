#ifndef CLICK_AGGREGATEFIRST_HH
#define CLICK_AGGREGATEFIRST_HH
#include <click/element.hh>
#include "aggregatenotifier.hh"
CLICK_DECLS

/*
=c

AggregateFirst([I<KEYWORDS>])

=s aggregates

lets through first packet per aggregate annotation

=d

AggregateFirst forwards only the first packet with a given aggregate
annotation value. Second and subsequent packets with that aggregate annotation
are emitted on the second output, if it exists, or dropped if it does not.

Keyword arguments are:

=over 8

=item NOTIFIER

The name of an AggregateNotifier element, like AggregateIPFlows. If given,
then AggregateFirst will prune information about old aggregates. This can save
significant memory on long traces.

=back

=n

AggregateLast forwards the last packet with a given aggregate annotation
value, and additionally annotates the packet with the observed packet and byte
counts. It takes significantly more memory, however.

Only available in user-level processes.

=a

AggregateLast, AggregateIP, AggregateIPFlows, AggregateCounter, AggregateFilter */

class AggregateFirst : public Element, public AggregateListener { public:

    AggregateFirst() CLICK_COLD;
    ~AggregateFirst() CLICK_COLD;

    const char *class_name() const	{ return "AggregateFirst"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    inline Packet *smaction(Packet *);
    void push(int, Packet *);
    Packet *pull(int);

    void aggregate_notify(uint32_t, AggregateEvent, const Packet *);

  private:

    enum { ROW_BITS = 10, ROW_SHIFT = 0, NROW = 1<<ROW_BITS, ROW_MASK = NROW - 1,
	   COL_BITS = 16, COL_SHIFT = ROW_BITS, NCOL = 1<<COL_BITS, COL_MASK = NCOL - 1,
	   PLANE_BITS = 32 - ROW_BITS - COL_BITS, PLANE_SHIFT = COL_SHIFT + COL_BITS, NPLANE = 1<<PLANE_BITS, PLANE_MASK = NPLANE - 1 };

    uint32_t **_kills[NPLANE];
    AggregateNotifier *_agg_notifier;
    uint32_t *_counts[NPLANE];

    uint32_t *create_row(uint32_t agg);
    inline uint32_t *row(uint32_t agg);

};

inline uint32_t *
AggregateFirst::row(uint32_t agg)
{
    if (uint32_t **p = _kills[(agg >> PLANE_SHIFT) & PLANE_MASK])
	if (uint32_t *c = p[(agg >> COL_SHIFT) & COL_MASK])
	    return c;
    return create_row(agg);
}

CLICK_ENDDECLS
#endif
