#ifndef IPREWRITER_HH
#define IPREWRITER_HH
#include "elements/ip/iprw.hh"

/*
 * =c
 * IPRewriter(INPUTSPEC1, ..., INPUTSPECn)
 * =s
 * rewrites UDP/TCP packets' addresses and ports
 * V<modifies>
 * =d
 *
 * Rewrites UDP and TCP flows by changing their source address, source port,
 * destination address, and/or destination port.
 *
 * Has one or more inputs and one or more outputs. Input packets must have
 * their IP header annotations set. Output packets are valid IP packets; for
 * instance, rewritten packets have their checksums incrementally updated.
 *
 * A flow is identified by its (source address, source port, destination
 * address, destination port) quadruple, called its I<flow identifier>.
 * IPRewriter maintains a set of I<mappings>, which say that one flow
 * identifier should be rewritten into another. A mapping consists of an input
 * flow ID, an output flow ID, a protocol, and an output port number. For
 * example, an IPRewriter might contain the mapping (1.0.0.1, 20, 2.0.0.2, 30)
 *  > (1.0.0.1, 20, 5.0.0.5, 80) with protocol TCP and output 4. If TCP packet
 * arrived on one of its input ports with flow ID (1.0.0.1, 20, 2.0.0.2, 30)
 * -- the packet is going from port 20 on machine 1.0.0.1, to port 30 on
 * machine 2.0.0.2 -- then the IPRewriter will change that packet's data so
 * the packet has flow ID (1.0.0.1, 20, 5.0.0.5, 80). The packet is now
 * heading to machine 5.0.0.5. It will then output the packet on output port
 * 4. Furthermore, if reply packets from 5.0.0.5 go through the IPRewriter,
 * they will be rewritten to look as if they came from 2.0.0.2; this
 * corresponds to a reverse mapping (5.0.0.5, 80, 1.0.0.1, 20) => (2.0.0.2,
 * 30, 1.0.0.1, 20).
 *
 * When it is first initialized, IPRewriter has no mappings. Mappings are
 * created on the fly as new flows are encountered in the form of packets with
 * unknown flow IDs. This process is controlled by the INPUTSPECs. There are
 * as many input ports as INPUTSPEC configuration arguments. Each INPUTSPEC
 * specifies whether and how a mapping should be created when a new flow is
 * encountered on the corresponding input port. There are five forms of
 * INPUTSPEC:
 *
 * =over 5
 * =item `drop'
 * Packets with no existing mapping are dropped.
 *
 * =item `nochange PORT'
 *
 * Packets with no existing mapping are sent to output port PORT. No mappings
 * are installed.
 *
 * =item `pattern SADDR SPORT DADDR DPORT FOUTPUT ROUTPUT'
 *
 * Packets with no existing mapping are rewritten according to the given
 * pattern, `SADDR SPORT DADDR DPORT'. The SADDR and DADDR portions may be
 * fixed IP addresses (in which case the corresponding packet field is set to
 * that address) or a dash `-' (in which case the corresponding packet field
 * is left unchanged). Similarly, DPORT is a port number or `-'. The SPORT
 * field may be a port number, `-', or a port range `L-H', in which case a
 * port number in the range L-H is chosen. IPRewriter makes sure that the
 * chosen port number was not used by any of that pattern's existing mappings.
 * If there is no such port number, the packet is dropped. (However, two
 * different patterns with matching SADDR, SPORT, and DADDR and overlapping
 * DPORT ranges might pick the same destination port number, resulting in an
 * ambiguous mapping. You should probably avoid this situation.)
 *
 * A new mapping is installed. Packets whose flow is like the input packet's
 * are rewritten and sent to FOUTPUT; packets in the reply flow are rewritten
 * and sent to ROUTPUT.
 *
 * =item `pattern PATNAME FOUTPUT ROUTPUT'
 *
 * Packets with no existing mapping are rewritten according to the pattern
 * PATNAME, which was specified in an IPRewriterPatterns element. Use this
 * form if two or more INPUTSPECs (possibly in different IPRewriter elements)
 * share a single port range, and reusing a live mapping would be an error.
 *
 * =item `ELEMENTNAME'
 *
 * Packets with no existing mapping are rewritten as the element ELEMENTNAME
 * suggests. When a new flow is detected, the element is given a chance to
 * provide a mapping for that flow. This is called a I<mapping request>. This
 * element must implement the IPMapper interface. One example mapper is
 * IPRoundRobinMapper.
 *
 * =back
 *
 * =h mappings read-only
 * Returns a human-readable description of the IPRewriter's current set of
 * mappings.
 *
 * =a IPRewriterPatterns, IPRoundRobinMapper, TCPRewriter, FTPPortMapper */

class IPRewriter : public IPRw {

  Map _tcp_map;
  Map _udp_map;

  Vector<InputSpec> _input_specs;
  Timer _timer;

  static const int GC_INTERVAL_SEC = 3600;

  static String dump_mappings_handler(Element *, void *);
  static String dump_patterns_handler(Element *, void *);
  
 public:

  IPRewriter();
  ~IPRewriter();
  
  const char *class_name() const		{ return "IPRewriter"; }
  void *cast(const char *);
  IPRewriter *clone() const			{ return new IPRewriter; }
  void notify_noutputs(int);
  const char *processing() const		{ return PUSH; }

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  void take_state(Element *, ErrorHandler *);
  
  void run_scheduled();

  Mapping *apply_pattern(Pattern *, int, int, bool tcp, const IPFlowID &);
  Mapping *get_mapping(bool, const IPFlowID &) const;
  
  void push(int, Packet *);

};


inline IPRw::Mapping *
IPRewriter::get_mapping(bool tcp, const IPFlowID &in) const
{
  if (tcp)
    return _tcp_map[in];
  else
    return _udp_map[in];
}

#endif
