#ifndef REWRITER_HH
#define REWRITER_HH
#include "element.hh"
#include "timer.hh"
#include "hashmap.hh"
#include "ipflowid.hh"
#include "click_ip.h"

/*
 * =c
 * Rewriter(PATTERN1, ..., PATTERNn)
 * =d
 *
 * Rewrites UDP and TCP streams according to the specified rewrite patterns.
 * Each UDP or TCP stream, as identified by the flow ID (source address,
 * source port, destination address, and destination port), has a separate
 * mapping. Rewriter will change the stream's flow ID to another arbitrary
 * flow ID. It can also perform the reverse mapping.
 * 
 * Has two inputs and an arbitrary number of outputs greater than or equal to
 * two.
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
 * is sent to output 0. The Rewriter itself does nothing with the other
 * PATTERNs. They are useful for other elements that manipulate Rewriter, such
 * as MappingCreator.
 *
 * Each mapping consists of an 9-tuple, mapping one flow ID (SADDR, SPORT,
 * DADDR, DPORT) onto another flow ID (SADDR', SPORT', DADDR', DPORT') and an
 * output number OUTPUT.
 *
 * Input 0 is the "forward" input. If a packet arrives on input 0 with
 * flow ID (SADDR, SPORT, DADDR, DPORT), Rewriter finds the corresponding
 * mapping, rewrites the packet's flow ID to (SADDR', SPORT', DADDR', DPORT'),
 * and pushes the packet to output OUTPUT. If there is no mapping, a new
 * mapping is created from the first pattern.
 *
 * Input 1 is the "reverse" input. If a packet arrives on input 1 with flow ID
 * (DADDR', DPORT', SADDR', SPORT'), Rewriter finds the corresponding mapping,
 * rewrites the packet's flow ID to (DADDR, DPORT, SADDR, SPORT), and pushes
 * the packet to the last output. If there is no reverse mapping, the packet
 * is dropped.
 * 
 * =a MappingCreator */

class MappingCreator;

class Rewriter : public Element {

  class Pattern;
  class Mapping;

  class Mapping {

    IPFlowID _mapto;
    
    unsigned short _ip_csum_incr;
    unsigned short _udp_csum_incr;

    Pattern *_pat;
    int _out;

    bool _used;
    bool _removed;

    void fix_csums(Packet *);

   public:

    Mapping();
    Mapping(const IPFlowID &, const IPFlowID &, Pattern *, int);

    const IPFlowID &flow_id() const		{ return _mapto; }
    Pattern *pattern() const			{ return _pat; }
    int output() const 				{ return _out; }
    bool used() const				{ return _used; }
    bool removed() const			{ return _removed; }

    void mark_used()				{ _used = true; }
    void reset_used()				{ _used = false; }
    void remove() 				{ _removed = true; }
    
    void apply(Packet *p);
    
  };

  class Pattern {
    
    // Pattern is <saddr/sport[-sport2]/daddr/dport>.
    // It is associated with a Rewriter output port.
    // It can be applied to a specific IPFlowID (<saddr/sport/daddr/dport>)
    // to obtain a new IPFlowID rewritten according to the Pattern.
    // Any Pattern component can be '*', which means that the corresponding
    // IPFlowID component is left unchanged. 
    IPAddress _saddr;
    int _sportl;		// host byte order
    int _sporth;		// host byte order
    IPAddress _daddr;
    int _dport;			// host byte order

    Vector<int> _free;
    void init_ports();

   public:
    
    Pattern();

    bool initialize(String &s);

    operator bool() const	{ return _saddr || _sporth || _daddr || _dport; }
    
    bool apply(const IPFlowID &in, IPFlowID &out);
    bool free(const IPFlowID &c);

    String s() const;
    operator String() const			{ return s(); }
    
  };

  Vector<Pattern *> _patterns;
  HashMap<IPFlowID, Mapping> _tcp_fwd;
  HashMap<IPFlowID, Mapping> _tcp_rev;
  HashMap<IPFlowID, Mapping> _udp_fwd;
  HashMap<IPFlowID, Mapping> _udp_rev;

  Timer _timer;

  static const int _gc_interval_sec = 10;

  void mark_live_tcp();
  void clean();

  void establish_mapping(const IPFlowID2 &, const IPFlowID &, Pattern *, int);

 public:

  Rewriter();
  ~Rewriter();
  
  const char *class_name() const		{ return "Rewriter"; }
  Rewriter *clone() const			{ return new Rewriter(); }
  void notify_noutputs(int);
  Processing default_processing() const		{ return PUSH; }
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  void run_scheduled();
  
  int npatterns() const				{ return _patterns.size(); }
  
  bool establish_mapping(Packet *, int pat, int output);
  bool establish_mapping(const IPFlowID2 &in, const IPFlowID &out, int output);
  const Mapping &get_mapping(const IPFlowID2 &in) const;
  
  void push(int, Packet *);
  
  String dump_table();
  String dump_patterns();
  
};

inline const Rewriter::Mapping &
Rewriter::get_mapping(const IPFlowID2 &in) const
{
  if (in.protocol() == IP_PROTO_TCP)
    return _tcp_fwd[in];
  else
    return _udp_fwd[in];
}

#endif REWRITER_HH
