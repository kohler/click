#ifndef IPADDRREWRITER_HH
#define IPADDRREWRITER_HH
#include "elements/ip/iprw.hh"

/*
=c

IPAddrRewriter(INPUTSPEC1, ..., INPUTSPECn)

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
IPAddrRewriter maintains a set of I<mappings>, which say that one flow
identifier should be rewritten into another. A mapping consists of an input
flow ID, an output flow ID, a protocol, and an output port number. For
example, an IPAddrRewriter might contain the mapping (1.0.0.1, 20, 2.0.0.2, 30)
=> (1.0.0.1, 20, 5.0.0.5, 80) with protocol TCP and output 4. Say a TCP
packet with flow ID (1.0.0.1, 20, 2.0.0.2, 30) arrived on one of that
IPAddrRewriter's input ports. (Thus, the packet is going from port 20 on
machine 1.0.0.1 to port 30 on machine 2.0.0.2.) Then the IPAddrRewriter will
change that packet's data so the packet has flow ID (1.0.0.1, 20, 5.0.0.5,
80). The packet is now heading to machine 5.0.0.5. It will then output the
packet on output port 4. Furthermore, if reply packets from 5.0.0.5 go
through the IPAddrRewriter, they will be rewritten to look as if they came from
2.0.0.2; this corresponds to a reverse mapping (5.0.0.5, 80, 1.0.0.1, 20)
=> (2.0.0.2, 30, 1.0.0.1, 20).

When it is first initialized, IPAddrRewriter has no mappings. Mappings are
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
port number in the range L-H is chosen. IPAddrRewriter makes sure that the
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
PATNAME, which was specified in an IPAddrRewriterPatterns element. Use this
form if two or more INPUTSPECs (possibly in different IPAddrRewriter elements)
share a single port range, and reusing a live mapping would be an error.

=item `ELEMENTNAME'

Packets with no existing mapping are rewritten as the element ELEMENTNAME
suggests. When a new flow is detected, the element is given a chance to
provide a mapping for that flow. This is called a I<mapping request>. This
element must implement the IPMapper interface. One example mapper is
RoundRobinIPMapper.

=back

IPAddrRewriter drops all fragments except the first, unless those fragments
arrived on an input port with a `nochange OUTPUT' specification. In that case,
the fragments are emitted on output OUTPUT.

=h mappings read-only

Returns a human-readable description of the IPAddrRewriter's current set of
mappings.

=a TCPRewriter, IPAddrRewriterPatterns, RoundRobinIPMapper, FTPPortMapper,
ICMPRewriter, ICMPPingRewriter */

class IPAddrRewriter : public IPRw { public:

  class IPAddrMapping : public Mapping { public:

    IPAddrMapping()		{ }

    IPAddrMapping *reverse() const { return static_cast<IPAddrMapping *>(_reverse); }

    void apply(WritablePacket *p);

    String s() const;
    
  };

  IPAddrRewriter();
  ~IPAddrRewriter();
  
  const char *class_name() const		{ return "IPAddrRewriter"; }
  void *cast(const char *);
  const char *processing() const		{ return PUSH; }
  IPAddrRewriter *clone() const			{ return new IPAddrRewriter; }
  void notify_noutputs(int);

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  //void take_state(Element *, ErrorHandler *);
  
  void run_scheduled();

  int notify_pattern(Pattern *, ErrorHandler *);
  IPAddrMapping *apply_pattern(Pattern *, int ip_p, const IPFlowID &, int, int);
  Mapping *get_mapping(int, const IPFlowID &) const;
  
  void push(int, Packet *);

  void add_handlers();

 private:

  Map _map;

  Vector<InputSpec> _input_specs;
  Timer _timer;

  static const int GC_INTERVAL_SEC = 7200;

  static String dump_mappings_handler(Element *, void *);
  static String dump_nmappings_handler(Element *, void *);
  static String dump_patterns_handler(Element *, void *);
  
};


inline IPRw::Mapping *
IPAddrRewriter::get_mapping(int, const IPFlowID &in) const
{
  IPFlowID flow_no_ports(in.saddr(), 0, in.daddr(), 0);
  return _map[flow_no_ports];
}

#endif
