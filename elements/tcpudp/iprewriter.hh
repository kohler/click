#ifndef CLICK_IPREWRITER_HH
#define CLICK_IPREWRITER_HH
#include "elements/ip/iprw.hh"
#include <click/sync.hh>
CLICK_DECLS

/*
=c

IPRewriter(INPUTSPEC1, ..., INPUTSPECn [, I<keywords>])

=s TCP

rewrites TCP/UDP packets' addresses and ports

=d

Rewrites the source address, source port, destination address, and/or
destination port on TCP and UDP packets, along with their checksums.
IPRewriter implements the functionality of a network address/port translator
E<lparen>NAPT).  See also IPAddrRewriter and IPAddrPairRewriter, which
implement Basic NAT, and TCPRewriter, which implements NAPT plus sequence
number changes for TCP packets.

IPRewriter maintains a I<mapping table> that records how packets are
rewritten.  The mapping table is indexed by I<flow identifier>, the quintuple
of source address, source port, destination address, destination port, and IP
protocol (TCP or UDP).  Each mapping contains a new flow identifier and an
output port.  Input packets with the indexed flow identifier are rewritten to
use the new flow identifier, then emitted on the output port.  A mapping is
written as follows:

    (SA, SP, DA, DP, PROTO) => (SA', SP', DA', DP') [OUTPUT]

When IPRewriter receives a packet, it first looks up that packet in the
mapping table by flow identifier.  If the table contains a mapping for the
input packet, then the packet is rewritten according to the mapping and
emitted on the specified output port.  If there was no mapping, the packet is
handled by the INPUTSPEC corresponding to the input port on which the packet
arrived.  (There are as many input ports as INPUTSPECs.)  Most INPUTSPECs
install new mappings, so that future packets from the same TCP or UDP flow are
handled by the mapping table rather than some INPUTSPEC.  The six forms of
INPUTSPEC handle input packets as follows:

=over 5

=item 'drop' or 'discard'

Discards input packets.

=item 'pass OUTPUT'

Sends input packets to output port OUTPUT.  No mappings are installed.

=item 'keep FOUTPUT ROUTPUT'

Installs mappings that preserve the input packet's flow ID.  Specifically,
given an input packet with flow ID (SA, SP, DA, DP, PROTO), two mappings are
installed:

    (SA, SP, DA, DP, PROTO) => (SA, SP, DA, DP) [FOUTPUT]
    (DA, DP, SA, SP, PROTO) => (DA, DP, SA, SP) [ROUTPUT]

Thus, the input packet is emitted on output port FOUTPUT unchanged, and
packets from the reply flow are emitted on output port ROUTPUT unchanged.

=item 'pattern SADDR SPORT DADDR DPORT FOUTPUT ROUTPUT'

Creates a mapping according to the given pattern, 'SADDR SPORT DADDR DPORT'.
Any pattern field may be a dash '-', in which case the packet's corresponding
field is left unchanged.  For instance, the pattern '1.0.0.1 20 - -' will
rewrite input packets' source address and port, but leave its destination
address and port unchanged.  SPORT may be a port range 'L-H'; IPRewriter will
choose a source port in that range so that the resulting mappings don't
conflict with any existing mappings.  If no source port is available, the
packet is dropped.  Normally source ports are chosen randomly within the
range.  To allocate source ports sequentially (which can make testing easier),
append a pound sign to the range, as in '1024-65535#'.

Say a packet with flow ID (SA, SP, DA, DP, PROTO) is received, and the
corresponding new flow ID is (SA', SP', DA', DP').  Then two mappings are
installed:

    (SA, SP, DA, DP, PROTO) => (SA', SP', DA', DP') [FOUTPUT]
    (DA', DP', SA', SP', PROTO) => (DA, DP, SA, SP) [ROUTPUT]

Thus, the input packet is rewritten and sent to FOUTPUT, and packets from the
reply flow are rewritten to look like part of the original flow and sent to
ROUTPUT.

=item 'pattern PATNAME FOUTPUT ROUTPUT'

Like 'pattern' above, but refers to named patterns defined by an
IPRewriterPatterns element.

=item 'ELEMENTNAME'

Creates mappings according to instructions from the element ELEMENTNAME.  This
element must implement the IPMapper interface.  One example mapper is
RoundRobinIPMapper.

=back

IPRewriter has no mappings when first initialized.

Input packets must have their IP header annotations set.  Non-TCP and UDP
packets, and second and subsequent fragments, are dropped unless they arrive
on a 'pass' input port.  IPRewriter changes IP packet data and, optionally,
destination IP address annotations; see the DST_ANNO keyword argument below.

Keyword arguments determine how often stale mappings should be removed.

=over 5

=item TCP_TIMEOUT I<time>

Time out TCP connections every I<time> seconds. Default is 24 hours.

=item TCP_DONE_TIMEOUT I<time>

Time out completed TCP connections every I<time> seconds. Default is 30
seconds. FIN and RST flags mark TCP connections as complete.

=item UDP_TIMEOUT I<time>

Time out UDP connections every I<time> seconds. Default is 1 minute.

=item REAP_TCP I<time>

Reap timed-out TCP connections every I<time> seconds. If no packets
corresponding to a given mapping have been seen for TCP_TIMEOUT, remove the
mapping as stale. Default is 1 hour.

=item REAP_TCP_DONE I<time>

Reap timed-out completed TCP connections every I<time> seconds. Default is 10
seconds.

=item REAP_UDP I<time>

Reap timed-out UDP connections every I<time> seconds. Default is 10 seconds.

=item DST_ANNO

Boolean. If true, then set the destination IP address annotation on passing
packets to the rewritten destination address. Default is true.

=back

=h tcp_mappings read-only

Returns a human-readable description of the IPRewriter's current set of
TCP mappings.

=h udp_mappings read-only

Returns a human-readable description of the IPRewriter's current set of
UDP mappings.

=h tcp_done_mappings read-only

Returns a human-readable description of the IPRewriter's current set of
mappings for completed TCP sessions.

=a TCPRewriter, IPAddrRewriter, IPAddrPairRewriter, IPRewriterPatterns,
RoundRobinIPMapper, FTPPortMapper, ICMPRewriter, ICMPPingRewriter */

#if defined(CLICK_LINUXMODULE) && __MTCLICK__
# define IPRW_SPINLOCKS 1
# define IPRW_RWLOCKS 0
#endif

class IPRewriter : public IPRw { public:

  IPRewriter();
  ~IPRewriter();
  
  const char *class_name() const		{ return "IPRewriter"; }
  void *cast(const char *);
  const char *port_count() const		{ return "1-/1-256"; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void take_state(Element *, ErrorHandler *);
  
  int notify_pattern(Pattern *, ErrorHandler *);
  Mapping *apply_pattern(Pattern *, int ip_p, const IPFlowID &, int, int);
  Mapping *get_mapping(int, const IPFlowID &) const;
  
  void push(int, Packet *);

  void add_handlers();
  int llrpc(unsigned, void *);

 private:
  
  Map _tcp_map;
  Map _udp_map;
  Mapping *_tcp_done;
  Mapping *_tcp_done_tail;

  Vector<InputSpec> _input_specs;
  bool _dst_anno;

  bool _tcp_done_gc_incr;
  int _tcp_done_gc_interval;
  int _tcp_gc_interval;
  int _udp_gc_interval;
  Timer _tcp_done_gc_timer;
  Timer _tcp_gc_timer;
  Timer _udp_gc_timer;
  int _udp_timeout_jiffies;
  int _tcp_timeout_jiffies;
  int _tcp_done_timeout_jiffies;

#if IPRW_SPINLOCKS
  Spinlock _spinlock;
#endif
#if IPRW_RWLOCKS
  ReadWriteLock _rwlock;
#endif

  int _nmapping_failures;
  
  static void tcp_gc_hook(Timer *, void *);
  static void udp_gc_hook(Timer *, void *);
  static void tcp_done_gc_hook(Timer *, void *);

  static String dump_mappings_handler(Element *, void *);
  static String dump_tcp_done_mappings_handler(Element *, void *);
  static String dump_nmappings_handler(Element *, void *);
  static String dump_patterns_handler(Element *, void *);
 
};


inline IPRw::Mapping *
IPRewriter::get_mapping(int ip_p, const IPFlowID &in) const
{
  if (ip_p == IP_PROTO_TCP)
    return _tcp_map[in];
  else if (ip_p == IP_PROTO_UDP)
    return _udp_map[in];
  else
    return 0;
}

CLICK_ENDDECLS
#endif
