#ifndef TRACE_HH
#define TRACE_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Trace([LIMIT, NO_CLONE, PREEMPT, IGNORE_GLOBAL_PREEMPT])

=s debugging

marks packets for debug tracing

=d

Marks up to LIMIT packets for debug tracing. With each subsequent push or pull
of such a packet, Click chatters a line indicating what element and port the
packet is pushed to or pulled from. It also chatters a line when the packet is
killed. By default, only one packet at a time (globally) can be traced: only
after the current traced packet is killed (or leaves Click) can a new packet be
marked for tracing. This behavior can be overridden using the 'preempt' or the
'global_preempt' write handlers.

Trace pushes cloned marked packets on the optional second output, which is
useful for printing marked packets. However, if NO_CLONE is set to true, no
clone is made.  Instead, the original unmarked and marked packets are pushed to
output 0 and 1, respectively. NO_CLONE is false by default and can only be set
to true in push context.

LIMIT is 1 by default and can be set to -1 to mark an unlimited number of
packets.

=e

  InfiniteSource(LIMIT 1)
  -> UDPIPEncap(10.0.0.10, 20000, 10.0.0.1, 3000)
  -> trace :: Trace
  => ( [0] -> [0]; [1] -> IPPrint(p, TIMESTAMP false) -> Discard; )
  -> Queue
  -> Unqueue
  -> Discard;

produces:

  traced_packet: started (trace :: Trace)
  p: 10.0.0.10.20000 > 10.0.0.1.3000: udp 77
  traced_packet: trace [0] -> [0] Queue@9
  traced_packet: Queue@9 [0] -> [0] Unqueue@10
  traced_packet: Unqueue@10 [0] -> [0] Discard@11
  traced_packet: killed

=n

Click must be built with HAVE_PACKET_TRACING in order for packet tracing to
work.N<>
N<>
Only pushes/pulls through a Port (which is the usual way) get printedN<>
N<>
Tracing does not transfer to packet clones: beware of broadcasts, Tee,
and the like.N<>

=h count read-only
Returns the number of packets that have been marked so far.

=h reset_count write-only
Resets the number of packets marked so far to 0. The element will then mark
another LIMIT packets.

=h limit read/write
Returns or sets the LIMIT parameter.

=h preempt read/write
Returns or sets the PREEMPT parameter. If PREEMPT is set, then Trace ignores the
any existing traced packet, allowing a new packet trace to preempt the existing
packet trace. This can be useful in cases where traced packets end up being
buffered e.g. by an ARPQuerier.

=h global_preempt read/write
Returns or sets the GLOBAL_PREEMPT value. If set, then all Trace elements
that do not have IGNORE_GLOBAL_PREEMPT set will preempt.

=h ignore_global_preempt read/write
Returns or sets the IGNORE_GLOBAL_PREEMPT parameter. If the parameter is set
then GLOBAL_PREEMPT is ignored.

=h poke write-only
Convenience handler that does everything needed to make the element print out
another packet: sets LIMIT to 1 if it is 0, and does a reset_count and
clear_traced.

=h global_clear_traced write-only
Globally clears packet marking state.  This handler is a safety in case the
global traced packet pointer does not get reset properly.

=h global_have_tracing read-only
Returns a boolean indicating whether Click has been built with packet tracing
enabled.
*/

class Trace : public Element { public:

    Trace() CLICK_COLD;
    ~Trace() CLICK_COLD;

    const char *class_name() const		{ return "Trace"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }

    void push(int, Packet *);
    Packet *pull(int);
    void add_handlers() CLICK_COLD;

  private:
    int _limit;
    unsigned _count;
    bool _no_clone;
    bool _preempt;
    bool _ignore_global_preempt;
    static bool _global_preempt;

    enum {
	h_reset_count,
	h_poke,
	h_clear_traced,
	h_have_tracing,
    };

    bool process(Packet *p);
    static int write_param(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;
    static String read_param(Element *e, void *thunk_p) CLICK_COLD;
};

CLICK_ENDDECLS
#endif
