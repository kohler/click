#ifndef IPREWRITER_HH
#define IPREWRITER_HH
#include "element.hh"
#include "timer.hh"
#include "bighashmap.hh"
#include "ipflowid.hh"
#include "click_ip.h"
class IPMapper;

/*
 * =c
 * IPRewriter(INPUTSPEC1, ..., INPUTSPECn)
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
 * address, destination port) quadruple, called its <i>flow identifier</i>.
 * IPRewriter maintains a set of <i>mappings</i>, which say that one flow
 * identifier should be rewritten into another. A mapping consists of an input
 * flow ID, an output flow ID, a protocol, and an output port number. For
 * example, an IPRewriter might contain the mapping (1.0.0.1, 20, 2.0.0.2, 30)
 * => (1.0.0.1, 20, 5.0.0.5, 80) with protocol TCP and output 4. If TCP packet
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
 * (`drop') Packets with no existing mapping are dropped.
 *
 * (`nochange PORT') Packets with no existing mapping are sent to output port
 * PORT. No mappings are installed.
 *
 * (`pattern SADDR SPORT DADDR DPORT FOUTPUT ROUTPUT') Packets with no
 * existing mapping are rewritten according to the given pattern, `SADDR SPORT
 * DADDR DPORT'. The SADDR and DADDR portions may be fixed IP addresses (in
 * which case the corresponding packet field is set to that address) or a dash
 * `-' (in which case the corresponding packet field is left unchanged).
 * Similarly, DPORT is a port number or `-'. The SPORT field may be a port
 * number, `-', or a port range `L-H', in which case a port number in the
 * range L-H is chosen. IPRewriter makes sure that the chosen port number was
 * not used by any of that pattern's existing mappings. If there is no such
 * port number, the packet is dropped. (However, two different patterns with
 * matching SADDR, SPORT, and DADDR and overlapping DPORT ranges might pick
 * the same destination port number, resulting in an ambiguous mapping. You
 * should probably avoid this situation.)
 *
 * A new mapping is installed. Packets whose flow is like the input packet's
 * are rewritten and sent to FOUTPUT; packets in the reply flow are rewritten
 * and sent to ROUTPUT.
 *
 * (`pattern PATNAME FOUTPUT ROUTPUT') Packets with no existing mapping are
 * rewritten according to the pattern PATNAME, which was specified in an
 * IPRewriterPatterns element. Use this form if two or more INPUTSPECs
 * (possibly in different IPRewriter elements) share a single port range, and
 * reusing a live mapping would be an error.
 *
 * (`ELEMENTNAME') Packets with no existing mapping are rewritten as the
 * element ELEMENTNAME suggests. When a new flow is detected, the element is
 * given a chance to provide a mapping for that flow. This is called a
 * <i>mapping request</i>. This element must implement the IPMapper interface.
 * One example mapper is IPRoundRobinMapper.
 *
 * =h mappings read-only
 * Returns a human-readable description of the IPRewriter's current set of
 * mappings.
 *
 * =a IPRewriterPatterns
 * =a IPRoundRobinMapper
 * =a FTPPortMapper */

class IPRewriter : public Element {

  class Pattern;
  class Mapping;

  enum InputSpecName {
    INPUT_SPEC_NOCHANGE, INPUT_SPEC_DROP, INPUT_SPEC_PATTERN, INPUT_SPEC_MAPPER
  };
  struct InputSpec {
    int kind;
    union {
      int output;
      struct {
	Pattern *p;
	int fport;
	int rport;
      } pattern;
      IPMapper *mapper;
    } u;
  };

  typedef BigHashMap<IPFlowID, Mapping *> Map;
  Vector<InputSpec> _input_specs;
  Map _tcp_map;
  Map _udp_map;

  Timer _timer;

  static const int GC_INTERVAL_SEC = 3600;

  void mark_live_tcp();
  void clean_map(Map &);
  void clean();
  void clear_map(Map &);

 public:

  IPRewriter();
  ~IPRewriter();
  
  const char *class_name() const		{ return "IPRewriter"; }
  IPRewriter *clone() const			{ return new IPRewriter; }
  void notify_noutputs(int);
  const char *processing() const		{ return PUSH; }

  enum ConfigurePhase {
    CONFIGURE_PHASE_PATTERNS = CONFIGURE_PHASE_INFO,
    CONFIGURE_PHASE_REWRITER = CONFIGURE_PHASE_DEFAULT,
    CONFIGURE_PHASE_USER = CONFIGURE_PHASE_REWRITER + 1,
  };

  int configure_phase() const		{ return CONFIGURE_PHASE_REWRITER; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  void run_scheduled();
  
  Mapping *get_mapping(const IPFlowID2 &in) const;
  void install(bool, Mapping *, Mapping *);
  
  void push(int, Packet *);
  
  String dump_table();
  String dump_patterns();

  class Mapping {
    
    IPFlowID _mapto;
    
    unsigned short _ip_csum_incr;
    unsigned short _udp_csum_incr;
    
    int _out;
    Mapping *_reverse;
    
    bool _used;
    bool _is_reverse;
    
    Pattern *_pat;
    Mapping *_pat_prev;
    Mapping *_pat_next;
    
    Mapping(const IPFlowID &, const IPFlowID &, Pattern *, int, bool);
    
   public:
    
    static bool make_pair(const IPFlowID &, const IPFlowID &, Pattern *,
			  int, int, Mapping **, Mapping **);
    
    const IPFlowID &flow_id() const	{ return _mapto; }
    Pattern *pattern() const		{ return _pat; }
    int output() const 			{ return _out; }
    bool is_forward() const		{ return !_is_reverse; }
    bool is_reverse() const		{ return _is_reverse; }
    Mapping *reverse() const		{ return _reverse; }
    bool used() const			{ return _used; }
    
    void mark_used()			{ _used = true; }
    void clear_used()			{ _used = false; }
    
    unsigned short sport() const	{ return _mapto.sport(); } // network
    
    void pat_insert_after(Mapping *);
    void pat_unlink();
    Mapping *pat_prev() const		{ return _pat_prev; }
    Mapping *pat_next() const		{ return _pat_next; }
    
    void apply(WritablePacket *p);
    
  };

  class Pattern {
    
    // Pattern is <saddr/sport[-sport2]/daddr/dport>.
    // It is associated with a IPRewriter output port.
    // It can be applied to a specific IPFlowID (<saddr/sport/daddr/dport>)
    // to obtain a new IPFlowID rewritten according to the Pattern.
    // Any Pattern component can be '-', which means that the corresponding
    // IPFlowID component is left unchanged. 
    IPAddress _saddr;
    int _sportl;			// host byte order
    int _sporth;			// host byte order
    IPAddress _daddr;
    int _dport;				// host byte order

    Mapping *_rover;		// walks along circular list ordered by port

    int _refcount;
    
    unsigned short find_sport();
    
   public:
    
    Pattern(const IPAddress &, int, int, const IPAddress &, int);
    static int parse(const String &, Pattern **, Element *, ErrorHandler *);
    static int parse_with_ports(const String &, Pattern **, int *, int *,
				Element *, ErrorHandler *);

    void use()				{ _refcount++; }
    void unuse()			{ if (--_refcount <= 0) delete this; }
    
    operator bool() const { return _saddr || _sporth || _daddr || _dport; }
    
    bool possible_conflict(const Pattern &) const;
    bool definite_conflict(const Pattern &) const;
    
    bool create_mapping(const IPFlowID &, int, int, Mapping **, Mapping **);
    void mapping_freed(Mapping *);
    
    String s() const;
    operator String() const			{ return s(); }
    
  };

};


class IPMapper {

 public:

  IPMapper()				{ }
  virtual ~IPMapper()			{ }

  void mapper_patterns(Vector<IPRewriter::Pattern *> &, IPRewriter *) const;
  IPRewriter::Mapping *get_map(bool, const IPFlowID &, IPRewriter *);
  
};


inline IPRewriter::Mapping *
IPRewriter::get_mapping(const IPFlowID2 &in) const
{
  if (in.protocol() == IP_PROTO_TCP)
    return _tcp_map[in];
  else
    return _udp_map[in];
}

inline void
IPRewriter::Mapping::pat_insert_after(Mapping *m)
{
  if (m) {
    _pat_prev = m;
    _pat_next = m->_pat_next;
    m->_pat_next = _pat_next->_pat_prev = this;
  } else
    _pat_prev = _pat_next = this;
}

#endif IPREWRITER_HH
