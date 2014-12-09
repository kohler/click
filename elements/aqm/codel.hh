#ifndef CLICK_CODEL_HH
#define CLICK_CODEL_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/timestamp.hh>
CLICK_DECLS
class Storage;

/*
=c

CoDel([, I<KEYWORDS>])

=s aqm

drops packets according to P<CoDel>

=d

Implements CoDel (Controlled Delay) active queue management
mechanism.

A CoDel element is associated with one or more Storage elements (usually
Queues). It tracks the sojourn time of every packet in the queue, and
increases the frequency of dropping packets until the queue is controlled,
i.e. the sojourn time goes below the threshold. The sojourn time is tracked
based on the "first timestamp" annotation, which must be set using the
SetTimestamp element just before the packet is enqueued. Later, CoDel overwrites
this annotation with the sojourn time experienced by the corresponding
packet for (possible) statistical use thereafter.

By default, the Queues are found with flow-based router context and only the
upstream queues are searched. CoDel is a pull element.

Arguments are:

=over 8

=item TARGET

Integer. Target sojourn time of the packet in the queue, default value is 5 ms.

=item INTERVAL

Integer. Sliding minimum window width, the default value is 100 ms.

=item QUEUES

This argument is a space-separated list of Storage element names. CoDel will use
those elements' queue lengths, rather than any elements found via flow-based
router context.

=back


=e

  ... -> SetTimestamp(FIRST true) -> Queue(200) -> CoDel(5, 100) -> ...


=h target read/write

Returns or sets the TARGET configuration parameter.

=h interval read/write

Returns or sets the INTERVAL configuration parameter.

=h queues read-only

Returns the Queues associated with this CoDel element, listed one per line.

=h drops read-only

Returns the number of packets dropped so far.

=h stats read-only

Returns some human-readable statistics.

=a Queue, SetTimestamp

Kathleen Nichols and Van Jacobson. I<Controlling Queue Delay>.
ACM Queue, 2012, vol.10, no.5. L<http://queue.acm.org/detail.cfm?id=2209336>

Appendix: CoDel Pseudocode. L<http://queue.acm.org/appendices/codel.html>. */

class CoDel : public Element { public:

    CoDel() CLICK_COLD;
    ~CoDel() CLICK_COLD;

    const char *class_name() const		{ return "CoDel"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return PULL; }


    int queue_size() const;
    int drops() const                           { return _total_drops; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    bool can_live_reconfigure() const           { return true; }
    void add_handlers() CLICK_COLD;

    void handle_drop(Packet *);
    Packet *pull(int port);

  protected:

    Storage *_queue1;
    Vector<Storage *> _queues;

    int _total_drops;
    int _state_drops;
    Timestamp _first_above_time, _drop_next;
    bool _dropping;
    bool _ok_to_drop;

    Timestamp _codel_interval_ts, _codel_target_ts;
    Vector<Element *> _queue_elements;

    Packet * delegate_codel();
    Timestamp control_law(Timestamp);
    Packet * dequeue_and_track_sojourn_time(Timestamp, bool &);
    static String read_handler(Element *, void *) CLICK_COLD;
    int finish_configure(const String &queues, ErrorHandler *errh);
};

CLICK_ENDDECLS
#endif
