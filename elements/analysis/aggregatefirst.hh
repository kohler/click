#ifndef CLICK_AGGREGATEUNIQ_HH
#define CLICK_AGGREGATEUNIQ_HH
#include <click/element.hh>
#include "aggregatenotifier.hh"
CLICK_DECLS

/*
=c

AggregateUniq([I<KEYWORDS>])

=s measurement

lets through one packet per aggregate annotation

=d

AggregateUniq forwards only the first packet it sees for each aggregate
annotation value. Second and subsequent packets with that aggregate annotation
are emitted on the second output, if it exists, or dropped if it does not.

Keyword arguments are:

=over 8

=item NOTIFIER

The name of an AggregateNotifier element, like AggregateIPFlows. If given,
then AggregateUniq will prune information about old aggregates. This can save
significant memory on long traces.

=back

=n

Only available in user-level processes.

=a

AggregateIP, AggregateIPFlows, AggregateCounter, AggregateFilter */

class AggregateUniq : public Element, public AggregateListener { public:
  
    AggregateUniq();
    ~AggregateUniq();
  
    const char *class_name() const	{ return "AggregateUniq"; }
    const char *processing() const	{ return "a/ah"; }
    AggregateUniq *clone() const	{ return new AggregateUniq; }

    void notify_noutputs(int);
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

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
AggregateUniq::row(uint32_t agg)
{
    if (uint32_t **p = _kills[(agg >> PLANE_SHIFT) & PLANE_MASK])
	if (uint32_t *c = p[(agg >> COL_SHIFT) & COL_MASK])
	    return c;
    return create_row(agg);
}

CLICK_ENDDECLS
#endif
