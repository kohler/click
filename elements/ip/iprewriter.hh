#ifndef IPREWRITER_HH
#define IPREWRITER_HH
#include "element.hh"
#include "timer.hh"
#include "hashmap.hh"
#include "ipflowid.hh"
#include "click_ip.h"

/*
 * =c
 * IPRewriter(PATTERN1, ..., PATTERNn)
 * =d
 *
 * Rewrites UDP and TCP packets by changing their source address, source port,
 * destination address, and/or destination port. The UDP and TCP packets are
 * identified by <i>flow identifier</i>: their source address/source
 * port/destination address/destination port quadruple. Two packets with the
 * same flow identifier will be consistently mapped to the same new flow
 * identifier. IPRewriter can also perform the reverse mapping.
 *
 * Has one or two inputs and one or more outputs.
 *
 * A mapping says that one flow ID (SADDR, SPORT, DADDR, DPORT) corresponds to
 * another (SADDR', SPORT', DADDR', DPORT'). Packets that arrive with the
 * first flow ID, (SADDR, SPORT, DADDR, DPORT), will be rewritten to the
 * second flow ID, (SADDR', SPORT', DADDR', DPORT'). Mappings come in pairs.
 * For every mapping (SADDR, SPORT, DADDR, DPORT) -> (SADDR', SPORT', DADDR',
 * DPORT'), there is another mapping (DADDR', DPORT', SADDR', SPORT') ->
 * (DADDR, DPORT, SADDR, SPORT), which changes replies to the rewritten
 * packets so they look like replies to the original packets.
 *
 * Each PATTERN describes a way to rewrite a packet's source and destination.
 * A PATTERN is either a single dash `-', meaning do no rewriting; or it has
 * four components, `SADDR SPORT DADDR DPORT'. Each component is either an IP
 * address or port number, meaning the rewritten packet will have that exact
 * address or port, or `-', meaning the rewritten packet's component will not
 * be changed. The SPORT component can also be a range `SPORT1-SPORT2',
 * meaning the rewriter will choose an unused port number in that range.
 *
 * The first PATTERN is special. When a packet that has no mapping arrives on
 * input 0, a new mapping is created corresponding to PATTERN1 and the packet
 * is sent to output 0. The IPRewriter itself does nothing with the other
 * PATTERNs. They are useful for other elements that manipulate IPRewriter.
 *
 * Input 0 is the "remap" input. If a packet arrives on input 0 with
 * flow ID (SADDR, SPORT, DADDR, DPORT), IPRewriter finds the corresponding
 * mapping, rewrites the packet's flow ID to (SADDR', SPORT', DADDR', DPORT'),
 * and pushes the packet to output OUTPUT. If there is no mapping, a new
 * mapping is created from the first pattern.
 *
 * Input 1 is like input 0, except that if a packet arrives with no mapping,
 * the packet is dropped.
 */

class IPRewriter : public Element {

  class Pattern;
  class Mapping;

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

    static void make_pair(const IPFlowID &, const IPFlowID &, Pattern *,
			  int, int, Mapping *&, Mapping *&);
    
    const IPFlowID &flow_id() const	{ return _mapto; }
    Pattern *pattern() const		{ return _pat; }
    int output() const 			{ return _out; }
    bool is_reverse() const		{ return _is_reverse; }
    Mapping *reverse() const		{ return _reverse; }
    bool used() const			{ return _used; }

    void mark_used()			{ _used = true; }
    void clear_used()			{ _used = false; }

    unsigned short sport() const	{ return _mapto.sport(); } // n.b.o.
    
    void pat_insert_after(Mapping *);
    void pat_unlink();
    Mapping *pat_prev() const		{ return _pat_prev; }
    Mapping *pat_next() const		{ return _pat_next; }

    void apply(Packet *p);
    
  };

  class Pattern {
    
    // Pattern is <saddr/sport[-sport2]/daddr/dport>.
    // It is associated with a IPRewriter output port.
    // It can be applied to a specific IPFlowID (<saddr/sport/daddr/dport>)
    // to obtain a new IPFlowID rewritten according to the Pattern.
    // Any Pattern component can be '*', which means that the corresponding
    // IPFlowID component is left unchanged. 
    IPAddress _saddr;
    int _sportl;		// host byte order
    int _sporth;		// host byte order
    IPAddress _daddr;
    int _dport;			// host byte order

    Mapping *_rover;		// walks along circular list ordered by port

    unsigned short find_sport();

   public:
    
    Pattern();

    bool initialize(String &s);
    void clear();
    bool possible_conflict(const Pattern &) const;
    bool definite_conflict(const Pattern &) const;

    operator bool() const { return _saddr || _sporth || _daddr || _dport; }
    
    bool create_mapping(const IPFlowID &, int, int, Mapping *&, Mapping *&);
    void mapping_freed(Mapping *);

    String s() const;
    operator String() const			{ return s(); }
    
  };

  Pattern *_patterns;
  int _npatterns;
  HashMap<IPFlowID, Mapping *> _tcp_map;
  HashMap<IPFlowID, Mapping *> _udp_map;

  Timer _timer;

  static const int _gc_interval_sec = 10;

  void install(int, Mapping *, Mapping *);
  void mark_live_tcp();
  void clean_one_map(HashMap<IPFlowID, Mapping *> &);
  void clean();

 public:

  IPRewriter();
  ~IPRewriter();
  
  const char *class_name() const		{ return "IPRewriter"; }
  IPRewriter *clone() const			{ return new IPRewriter; }
  void notify_ninputs(int);
  void notify_noutputs(int);
  const char *processing() const		{ return PUSH; }
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  void run_scheduled();
  
  int npatterns() const				{ return _npatterns; }
  
  Mapping *establish_mapping(Packet *, int, int output);
  Mapping *establish_mapping(const IPFlowID2 &, const IPFlowID &, int output);
  Mapping *get_mapping(const IPFlowID2 &in) const;
  
  void push(int, Packet *);
  
  String dump_table();
  String dump_patterns();
  
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
