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

AggregateCounter maintains counts of how many packets or bytes it has seen for
each aggregate value. Each aggregate annotation value gets a different count.
Call its C<write_file> or C<write_ascii_file> write handler to get a dump of
the information.

The C<freeze> handler, and the C<AGGREGATE_FREEZE> and C<COUNT_FREEZE>
keyword arguments, can put AggregateCounter in a frozen state. Frozen
AggregateCounters only update existing counters; they do not create new
counters for previously unseen aggregate values.

AggregateCounter may have one or two inputs. The optional second input is
always frozen. (It is only useful when the element is push.) It may also have
two outputs. If so, and the element is push, then packets that were counted
are emitted on the first output, while other packets are emitted on the second
output.

Keyword arguments are:

=over 8

=item NOTIFIER

The name of an AggregateNotifier element, like AggregateIPFlows. If given,
then ToIPFlowDumps will ask the element for notification when flows are
deleted. It uses that notification to free its state early. It's a very good
idea to supply a NOTIFIER.

=back


=n

The aggregate identifier is stored in host byte order. Thus, the aggregate ID
corresponding to IP address 128.0.0.0 is 2147483648.

Only available in user-level processes.

=a

AggregateIP, FromIPSummaryDump, FromDump, tcpdpriv(1) */

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
