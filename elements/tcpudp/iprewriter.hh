#ifndef IPREWRITER_HH
#define IPREWRITER_HH
#include "elements/ip/iprw.hh"
#include <click/sync.hh>

/*
=c

IPRewriter(INPUTSPEC1, ..., INPUTSPECn [, KEYWORDS])

=s TCP

rewrites UDP/TCP packets' addresses and ports

=d

Rewrites UDP and TCP flows by changing their source address, source port,
destination address, and/or destination port.

Has one or more inputs and one or more outputs. Input packets must have
their IP header annotations set. Output packets are valid IP packets; for
instance, rewritten packets have their checksums incrementally updated.

A flow is identified by its (source address, source port, destination
address, destination port) quadruple, called its I<flow identifier>.
IPRewriter maintains a set of I<mappings>, which say that one flow
identifier should be rewritten into another. A mapping consists of an input
flow ID, an output flow ID, a protocol, and an output port number. For
example, an IPRewriter might contain the mapping (1.0.0.1, 20, 2.0.0.2, 30)
=> (1.0.0.1, 20, 5.0.0.5, 80) with protocol TCP and output 4. Say a TCP
packet with flow ID (1.0.0.1, 20, 2.0.0.2, 30) arrived on one of that
IPRewriter's input ports. (Thus, the packet is going from port 20 on
machine 1.0.0.1 to port 30 on machine 2.0.0.2.) Then the IPRewriter will
change that packet's data so the packet has flow ID (1.0.0.1, 20, 5.0.0.5,
80). The packet is now heading to machine 5.0.0.5. It will then output the
packet on output port 4. Furthermore, if reply packets from 5.0.0.5 go
through the IPRewriter, they will be rewritten to look as if they came from
2.0.0.2; this corresponds to a reverse mapping (5.0.0.5, 80, 1.0.0.1, 20)
=> (2.0.0.2, 30, 1.0.0.1, 20).

When it is first initialized, IPRewriter has no mappings. Mappings are
created on the fly as new flows are encountered in the form of packets with
unknown flow IDs. This process is controlled by the INPUTSPECs. There are
as many input ports as INPUTSPEC configuration arguments. Each INPUTSPEC
specifies whether and how a mapping should be created when a new flow is
encountered on the corresponding input port. There are five forms of
INPUTSPEC:

=over 5

=item `drop'

Packets with no existing mapping are dropped.

=item `nochange OUTPUT'

Packets with no existing mapping are sent to output port OUTPUT. No mappings
are installed.

=item `keep FOUTPUT ROUTPUT'

Packets with no existing mapping are sent to output port FOUTPUT. A mapping
is installed that keeps the packet's flow ID the same. Reply packets are
mapped to ROUTPUT.

=item `pattern SADDR SPORT DADDR DPORT FOUTPUT ROUTPUT'

Packets with no existing mapping are rewritten according to the given
pattern, `SADDR SPORT DADDR DPORT'. The SADDR and DADDR portions may be
fixed IP addresses (in which case the corresponding packet field is set to
that address) or a dash `-' (in which case the corresponding packet field
is left unchanged). Similarly, DPORT is a port number or `-'. The SPORT
field may be a port number, `-', or a port range `L-H', in which case a
port number in the range L-H is chosen. IPRewriter makes sure that the
chosen port number was not used by any of that pattern's existing mappings.
If there is no such port number, the packet is dropped. (However, two
different patterns with matching SADDR, SPORT, and DADDR and overlapping
DPORT ranges might pick the same destination port number, resulting in an
ambiguous mapping. You should probably avoid this situation.)

A new mapping is installed. Packets whose flow is like the input packet's
are rewritten and sent to FOUTPUT; packets in the reply flow are rewritten
and sent to ROUTPUT.

=item `pattern PATNAME FOUTPUT ROUTPUT'

Packets with no existing mapping are rewritten according to the pattern
PATNAME, which was specified in an IPRewriterPatterns element. Use this
form if two or more INPUTSPECs (possibly in different IPRewriter elements)
share a single port range, and reusing a live mapping would be an error.

=item `ELEMENTNAME'

Packets with no existing mapping are rewritten as the element ELEMENTNAME
suggests. When a new flow is detected, the element is given a chance to
provide a mapping for that flow. This is called a I<mapping request>. This
element must implement the IPMapper interface. One example mapper is
RoundRobinIPMapper.

=back

IPRewriter drops all fragments except the first, unless those fragments
arrived on an input port with a `nochange OUTPUT' specification. In that case,
the fragments are emitted on output OUTPUT.

Keyword arguments determine how often stale mappings should be removed.

=over 5

=item REAP_TCP I<time>

Reap TCP connections every I<time> seconds. If no packets corresponding to a
given mapping have been seen for TCP_TIMEOUT, remove the mapping as stale.
Default is 1 hour.

=item REAP_TCP_DONE I<time>

Reap completed TCP connections every I<time> seconds. FIN or RST flags mark a
TCP connection as complete. Default is 10 seconds.

=item REAP_UDP I<time>

Reap UDP connections every I<time> seconds. Default is 10 seconds.

=item TCP_TIMEOUT I<time>

Timeout a TCP connections every I<time> seconds. Default is 24 hours.

=item TCP_DONE_TIMEOUT I<time>

Timeout a completed TCP connections every I<time> seconds. 
Default is 30 seconds.

=item UDP_TIMEOUT I<time>

Timeout a UDP connections every I<time> seconds. Default is 1 minute.


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

=a TCPRewriter, IPRewriterPatterns, RoundRobinIPMapper, FTPPortMapper,
ICMPRewriter, ICMPPingRewriter */

#if defined(__KERNEL__) && __MTCLICK__
# define IPRW_SPINLOCKS 1
# define IPRW_RWLOCKS 0
#endif

class IPRewriter : public IPRw { public:

  IPRewriter();
  ~IPRewriter();
  
  const char *class_name() const		{ return "IPRewriter"; }
  void *cast(const char *);
  const char *processing() const		{ return PUSH; }
  IPRewriter *clone() const			{ return new IPRewriter; }
  void notify_noutputs(int);

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
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

  bool _tcp_done_gc_incr;
  int _tcp_done_gc_interval;
  int _tcp_gc_interval;
  int _udp_gc_interval;
  Timer _tcp_done_gc_timer;
  Timer _tcp_gc_timer;
  Timer _udp_gc_timer;
  int _udp_timeout_interval;
  int _tcp_timeout_interval;
  int _tcp_done_timeout_interval;
  int _instance_index;

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
 
  static int _global_instance_counter;
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

#endif
