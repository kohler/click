// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPRW_HH
#define CLICK_IPRW_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/bighashmap.hh>
#include <click/ipflowid.hh>
#include <clicknet/ip.h>
CLICK_DECLS
class IPMapper;

class IPRw : public Element { public:

    class Pattern;
    class Mapping;
    typedef BigHashMap<IPFlowID, Mapping *> Map;
    enum InputSpecName {
	INPUT_SPEC_NOCHANGE, INPUT_SPEC_KEEP, INPUT_SPEC_DROP,
	INPUT_SPEC_PATTERN, INPUT_SPEC_MAPPER
    };
    struct InputSpec {
	int kind;
	union {
	    int output;
	    struct {
		int fport;
		int rport;
	    } keep;
	    struct {
		Pattern *p;
		int fport;
		int rport;
	    } pattern;
	    IPMapper *mapper;
	} u;
    };

    IPRw();
    ~IPRw();

    enum ConfigurePhase {
	CONFIGURE_PHASE_PATTERNS = CONFIGURE_PHASE_INFO,
	CONFIGURE_PHASE_REWRITER = CONFIGURE_PHASE_DEFAULT,
	CONFIGURE_PHASE_MAPPER = CONFIGURE_PHASE_REWRITER - 1,
	CONFIGURE_PHASE_USER = CONFIGURE_PHASE_REWRITER + 1
    };

    int configure_phase() const 	{ return CONFIGURE_PHASE_REWRITER; }

    int parse_input_spec(const String &, InputSpec &, String, ErrorHandler *);

    virtual int notify_pattern(Pattern *, ErrorHandler *);
    virtual Mapping *apply_pattern(Pattern *, int ip_p, const IPFlowID &, int, int) = 0;
    virtual Mapping *get_mapping(int ip_p, const IPFlowID &) const = 0;

  protected:

    Vector<Pattern *> _all_patterns;

    enum { GC_INTERVAL_SEC = 3600 };

    void take_state_map(Map &, Mapping **free_head, Mapping **free_tail,
			const Vector<Pattern *> &, const Vector<Pattern *> &);
    void clear_map(Map &);
    void clean_map(Map &, uint32_t last_jif);
    void clean_map_free_tracked(Map&, Mapping *&free_head, Mapping *&free_tail,
				uint32_t last_jif);
    void incr_clean_map_free_tracked(Map &, Mapping *&head, Mapping *&tail,
				     uint32_t last_jif);

};


class IPRw::Mapping { public:

    Mapping(bool dst_anno);

    void initialize(int ip_p, const IPFlowID &, const IPFlowID &, int, bool, Mapping *);
    static void make_pair(int ip_p, const IPFlowID &, const IPFlowID &,
			  int, int, Mapping *, Mapping *);

    const IPFlowID &flow_id() const	{ return _mapto; }
    Pattern *pattern() const		{ return _pat; }
    int output() const 			{ return _output; }
    bool is_primary() const		{ return !_is_reverse; }
    const Mapping *primary() const { return is_primary() ? this : _reverse; }
    Mapping *primary()		   { return is_primary() ? this : _reverse; }
    Mapping *reverse() const		{ return _reverse; }

    bool used_since(uint32_t) const;
    void mark_used()			{ _used = click_jiffies(); }

    bool marked() const			{ return _marked; }
    void mark()				{ _marked = true; }
    void unmark()			{ _marked = false; }

    uint16_t sport() const		{ return _mapto.sport(); } // network

    void pat_insert_after(Pattern *, Mapping *);
    void pat_unlink();
    Mapping *pat_prev() const		{ return _pat_prev; }
    Mapping *pat_next() const		{ return _pat_next; }

    Mapping *free_next() const		{ return _free_next; }
    void set_free_next(Mapping *m)	{ _free_next = m; }

    bool session_over() const		{ return _flow_over && _reverse->_flow_over; }
    void set_session_over()		{ _flow_over = _reverse->_flow_over = true; }
    void set_session_flow_over()	{ _flow_over = true; }
    void clear_session_flow_over()	{ _flow_over = false; }

    bool free_tracked() const		{ return _free_tracked; }
    void add_to_free_tracked_tail(Mapping *&head, Mapping *&tail);
    void clear_free_tracked();

    void apply(WritablePacket *);

    String s() const;
  
  protected:

    IPFlowID _mapto;

    uint16_t _ip_csum_delta;
    uint16_t _udp_csum_delta;

    Mapping *_reverse;

    bool _is_reverse : 1;
    bool _marked : 1;
    bool _flow_over : 1;
    bool _free_tracked : 1;
    bool _dst_anno : 1;
    uint8_t _ip_p;
    uint8_t _output;
    unsigned _used;

    Pattern *_pat;
    Mapping *_pat_prev;
    Mapping *_pat_next;

    Mapping *_free_next;

    friend class IPRw;

    inline Mapping *free_from_list(Map &, bool notify);
    void append_to_free(Mapping *&head, Mapping *&tail);

};


class IPRw::Pattern { public:

    // Pattern is <saddr/sport[-sport2]/daddr/dport>.
    // It is associated with a IPRewriter output port.
    // It can be applied to a specific IPFlowID (<saddr/sport/daddr/dport>)
    // to obtain a new IPFlowID rewritten according to the Pattern.
    // Any Pattern component can be '-', which means that the corresponding
    // IPFlowID component is left unchanged. 

    Pattern(const IPAddress &, int, int, const IPAddress &, int);
    static int parse(const String &, Pattern **, Element *, ErrorHandler *);
    static int parse_with_ports(const String &, Pattern **,
				int *, int *, Element *, ErrorHandler *);

    void use()			{ _refcount++; }
    void unuse()		{ if (--_refcount <= 0) delete this; }

    int nmappings() const	{ return _nmappings; }
  
    operator bool() const { return _saddr || _sporth || _daddr || _dport; }
    bool allow_nat() const	{ return !_is_napt || _sportl == _sporth; }
    bool allow_napt() const	{ return _is_napt || _sportl == _sporth; }

    bool can_accept_from(const Pattern &) const;

    bool create_mapping(int ip_p, const IPFlowID &, int fport, int rport, Mapping *, Mapping *);
    void accept_mapping(Mapping *);
    void mapping_freed(Mapping *);

    Mapping *rover()		{ return _rover; }

    String s() const;
    operator String() const	{ return s(); }

  public:

    IPAddress _saddr;
    int _sportl;		// host byte order
    int _sporth;		// host byte order
    IPAddress _daddr;
    int _dport;			// host byte order

    Mapping *_rover;		// walks along circular list ordered by port
    bool _is_napt;

    int _refcount;
    int _nmappings;

    Pattern(const Pattern &);
    Pattern &operator=(const Pattern &);

    unsigned short find_sport();

    static int parse_napt(Vector<String> &, Pattern **, Element *, ErrorHandler *);
    static int parse_nat(Vector<String> &, Pattern **, Element *, ErrorHandler *);
    friend class Mapping;

};


class IPMapper { public:

    IPMapper()				{ }
    virtual ~IPMapper()			{ }

    void notify_rewriter(IPRw *, ErrorHandler *);
    IPRw::Mapping *get_map(IPRw *, int ip_p, const IPFlowID &, Packet *);

};


inline void
IPRw::Mapping::pat_insert_after(Pattern *p, Mapping *m)
{
    _pat = p;
    if (m) {
	_pat_prev = m;
	_pat_next = m->_pat_next;
	m->_pat_next = _pat_next->_pat_prev = this;
	p->_nmappings++;
    } else
	_pat_prev = _pat_next = this;
}

inline void
IPRw::Mapping::pat_unlink()
{
    if (_pat) {
	_pat_next->_pat_prev = _pat_prev;
	_pat_prev->_pat_next = _pat_next;
	_pat->_nmappings--;
    }
}

inline void
IPRw::Mapping::append_to_free(Mapping *&head, Mapping *&tail)
{
    assert((!head && !tail)
	   || (head && tail && head->_free_tracked && tail->_free_tracked));
    assert(!_free_next && !_reverse->_free_next);
    if (tail)
	tail = tail->_free_next = this;
    else
	head = tail = this;
}

inline void
IPRw::Mapping::add_to_free_tracked_tail(Mapping *&head, Mapping *&tail)
{
    assert(!_free_tracked && !_reverse->_free_tracked);
    _free_tracked = _reverse->_free_tracked = true;
    primary()->append_to_free(head, tail);
}

inline void
IPRw::Mapping::clear_free_tracked()
{
    _free_tracked = _reverse->_free_tracked = false;
    _free_next = 0;
    assert(_reverse->_free_next == 0);
}

inline bool
IPRw::Mapping::used_since(uint32_t t) const
{ 
    return ((int32_t)(_used - t)) >= 0 || ((int32_t)(_reverse->_used - t)) >= 0;
}

CLICK_ENDDECLS
#endif
