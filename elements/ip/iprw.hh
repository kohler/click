#ifndef IPRW_HH
#define IPRW_HH
#include "element.hh"
#include "timer.hh"
#include "bighashmap.hh"
#include "ipflowid.hh"
#include "click_ip.h"
class IPMapper;

class IPRw : public Element { protected:

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
  
  Vector<Pattern *> _all_patterns;

  static const int GC_INTERVAL_SEC = 3600;

  void take_state_map(Map &, const Vector<Pattern *> &, const Vector<Pattern *> &);
  void mark_live_tcp(Map &);
  void clean_map(Map &);
  void clear_map(Map &);

 public:

  IPRw();
  ~IPRw();
  
  enum ConfigurePhase {
    CONFIGURE_PHASE_PATTERNS = CONFIGURE_PHASE_INFO,
    CONFIGURE_PHASE_REWRITER = CONFIGURE_PHASE_DEFAULT,
    CONFIGURE_PHASE_MAPPER = CONFIGURE_PHASE_REWRITER - 1,
    CONFIGURE_PHASE_USER = CONFIGURE_PHASE_REWRITER + 1,
  };

  int configure_phase() const		{ return CONFIGURE_PHASE_REWRITER; }
  
  int parse_input_spec(const String &, InputSpec &, String, ErrorHandler *);
  
  virtual void notify_pattern(Pattern *);
  virtual Mapping *apply_pattern(Pattern *, int, int, bool tcp, const IPFlowID &) = 0;
  virtual Mapping *get_mapping(bool tcp, const IPFlowID &) const = 0;
  
};


class IPRw::Mapping { protected:
    
  IPFlowID _mapto;
    
  unsigned short _ip_csum_delta;
  unsigned short _udp_csum_delta;
    
  int _output;
  Mapping *_reverse;
    
  bool _used;
  bool _is_reverse;
    
  Pattern *_pat;
  Mapping *_pat_prev;
  Mapping *_pat_next;
    
 public:

  Mapping();
  
  void initialize(const IPFlowID &, const IPFlowID &, int, bool, Mapping *);
  static void make_pair(const IPFlowID &, const IPFlowID &,
			int, int, Mapping *, Mapping *);
    
  const IPFlowID &flow_id() const	{ return _mapto; }
  Pattern *pattern() const		{ return _pat; }
  int output() const 			{ return _output; }
  bool is_forward() const		{ return !_is_reverse; }
  bool is_reverse() const		{ return _is_reverse; }
  Mapping *reverse() const		{ return _reverse; }
  bool used() const			{ return _used; }
  
  void mark_used()			{ _used = true; }
  void clear_used()			{ _used = false; }
    
  unsigned short sport() const		{ return _mapto.sport(); } // network
  
  void pat_insert_after(Pattern *, Mapping *);
  void pat_unlink();
  Mapping *pat_prev() const		{ return _pat_prev; }
  Mapping *pat_next() const		{ return _pat_next; }
    
  void apply(WritablePacket *p);

  String s() const;
    
};


class IPRw::Pattern {
  
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
  int _dport;			// host byte order

  Mapping *_rover;		// walks along circular list ordered by port

  int _refcount;

  Pattern(const Pattern &);
  Pattern &operator=(const Pattern &);
  
  unsigned short find_sport();
    
 public:
    
  Pattern(const IPAddress &, int, int, const IPAddress &, int);
  static int parse(const String &, Pattern **, Element *, ErrorHandler *);
  static int parse_with_ports(const String &, Pattern **, int *, int *,
			      Element *, ErrorHandler *);

  void use()			{ _refcount++; }
  void unuse()			{ if (--_refcount <= 0) delete this; }
  
  operator bool() const { return _saddr || _sporth || _daddr || _dport; }
    
  bool can_accept_from(const Pattern &) const;

  bool create_mapping(const IPFlowID &, int, int, Mapping *, Mapping *);
  void accept_mapping(Mapping *);
  void mapping_freed(Mapping *);
    
  String s() const;
  operator String() const			{ return s(); }
    
};


class IPMapper {

 public:

  IPMapper()				{ }
  virtual ~IPMapper()			{ }

  void notify_rewriter(IPRw *, ErrorHandler *);
  IPRw::Mapping *get_map(IPRw *, bool, const IPFlowID &);
  
};


inline void
IPRw::Mapping::pat_insert_after(Pattern *p, Mapping *m)
{
  _pat = p;
  if (m) {
    _pat_prev = m;
    _pat_next = m->_pat_next;
    m->_pat_next = _pat_next->_pat_prev = this;
  } else
    _pat_prev = _pat_next = this;
}

inline void
IPRw::Mapping::pat_unlink()
{
  _pat_next->_pat_prev = _pat_prev;
  _pat_prev->_pat_next = _pat_next;
}

#endif
