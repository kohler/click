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
    typedef HashMap<IPFlowID, Mapping*> Map;
    enum InputSpecName {
	INPUT_SPEC_NOCHANGE, INPUT_SPEC_KEEP, INPUT_SPEC_DROP,
	INPUT_SPEC_PATTERN, INPUT_SPEC_MAPPER
    };
    struct InputSpec {
	int kind;
	union {
	    int output;
	    struct {
		Pattern* p;
		int fport;
		int rport;
	    } pattern;		/* also used for INPUT_SPEC_KEEP */
	    IPMapper* mapper;
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

    int parse_input_spec(const String&, InputSpec&, String, ErrorHandler*);

    virtual int notify_pattern(Pattern*, ErrorHandler*);
    virtual Mapping* apply_pattern(Pattern*, int ip_p, const IPFlowID&, int, int) = 0;
    virtual Mapping* get_mapping(int ip_p, const IPFlowID&) const = 0;

  protected:

    Vector<Pattern*> _all_patterns;

    enum { GC_INTERVAL_SEC = 3600 };

    void take_state_map(Map&, Mapping** free_head, Mapping** free_tail,
			const Vector<Pattern*>&, const Vector<Pattern*>&);
    void clear_map(Map&);
    void clean_map(Map&, uint32_t last_jif);
    void clean_map_free_tracked(Map&, Mapping*& free_head, Mapping*& free_tail,
				uint32_t last_jif);
    void incr_clean_map_free_tracked(Map&, Mapping*& head, Mapping*& tail,
				     uint32_t last_jif);

};


class IPRw::Mapping { public:

    enum { F_REVERSE = 1, F_MARKED = 2, F_FLOW_OVER = 4, F_FREE_TRACKED = 8,
	   F_DST_ANNO = 16 };

    Mapping(bool dst_anno);

    void initialize(int ip_p, const IPFlowID& inf, const IPFlowID& outf, int output, uint16_t flags, Mapping* reverse);
    static void make_pair(int ip_p, const IPFlowID& inf, const IPFlowID& outf,
			  int foutput, int routput, Mapping* f, Mapping* r);

    const IPFlowID& flow_id() const	{ return _mapto; }
    Pattern* pattern() const		{ return _pat; }
    int output() const 			{ return _output; }
    bool is_primary() const		{ return !(_flags & F_REVERSE); }
    const Mapping* primary() const { return is_primary() ? this : _reverse; }
    Mapping* primary()		   { return is_primary() ? this : _reverse; }
    Mapping* reverse() const		{ return _reverse; }

    inline bool used_since(uint32_t) const;
    void mark_used()			{ _used = click_jiffies(); }

    bool marked() const			{ return (_flags & F_MARKED); }
    void mark()				{ _flags |= F_MARKED; }
    void unmark()			{ _flags &= ~F_MARKED; }

    uint16_t sport() const		{ return _mapto.sport(); } // network

    Mapping* free_next() const		{ return _free_next; }
    void set_free_next(Mapping* m)	{ _free_next = m; }

    bool session_over() const		{ return (_flags & F_FLOW_OVER) && (_reverse->_flags & F_FLOW_OVER); }
    void set_session_over()		{ _flags |= F_FLOW_OVER; _reverse->_flags |= F_FLOW_OVER; }
    void set_session_flow_over()	{ _flags |= F_FLOW_OVER; }
    void clear_session_flow_over()	{ _flags &= ~F_FLOW_OVER; }

    bool free_tracked() const		{ return (_flags & F_FREE_TRACKED); }
    inline void add_to_free_tracked_tail(Mapping*& head, Mapping*& tail);
    inline void clear_free_tracked();

    void apply(WritablePacket*);

    String unparse() const;

  protected:

    IPFlowID _mapto;

    uint16_t _ip_csum_delta;
    uint16_t _udp_csum_delta;

    Mapping* _reverse;

    uint16_t _flags;
    uint8_t _ip_p;
    uint8_t _output;
    unsigned _used;

    // these variables maintained by IPRw::Pattern
    Pattern* _pat;

    Mapping* _free_next;

    friend class IPRw;
    friend class IPRw::Pattern;

    inline Mapping* free_from_list(Map&, bool notify);
    inline void append_to_free(Mapping*& head, Mapping*& tail);

};


class IPRw::Pattern { public:

    // Pattern is <saddr/sport[-sport2]/daddr/dport>.
    // It is associated with a IPRewriter output port.
    // It can be applied to a specific IPFlowID (<saddr/sport/daddr/dport>)
    // to obtain a new IPFlowID rewritten according to the Pattern.
    // Any Pattern component can be '-', which means that the corresponding
    // IPFlowID component is left unchanged. 

    Pattern(const IPAddress&, int, const IPAddress&, int, bool is_napt, bool sequential, uint32_t variation);
    static int parse(const String&, Pattern**, Element*, ErrorHandler*);
    static int parse_with_ports(const String&, Pattern**,
				int*, int*, Element*, ErrorHandler*);

    void use()			{ _refcount++; }
    void unuse()		{ if (--_refcount <= 0) delete this; }

    int nmappings() const	{ return _nmappings; }
  
    operator bool() const	{ return _saddr || _sport || _daddr || _dport; }
    IPAddress daddr() const	{ return _daddr; }
    bool allow_nat() const	{ return !_is_napt || _variation_top == 0; }
    bool allow_napt() const	{ return _is_napt || _variation_top == 0; }

    bool can_accept_from(const Pattern&) const;

    bool create_mapping(int ip_p, const IPFlowID&, int fport, int rport, Mapping*, Mapping*, const Map&);
    void accept_mapping(Mapping*);
    inline void mapping_freed(Mapping*);

    String unparse() const;

  public:

    IPAddress _saddr;
    int _sport;			// host byte order
    IPAddress _daddr;
    int _dport;			// host byte order

    uint32_t _variation_top;
    uint32_t _variation_mask;
    uint32_t _next_variation;

    bool _is_napt;
    bool _sequential;

    int _refcount;
    int _nmappings;

    Pattern(const Pattern&);
    Pattern& operator=(const Pattern&);

    static int parse_napt(Vector<String>&, Pattern**, Element*, ErrorHandler*);
    static int parse_nat(Vector<String>&, Pattern**, Element*, ErrorHandler*);
    friend class Mapping;

};


class IPMapper { public:

    IPMapper()				{ }
    virtual ~IPMapper()			{ }

    void notify_rewriter(IPRw*, ErrorHandler*);
    virtual IPRw::Mapping* get_map(IPRw*, int ip_p, const IPFlowID&, Packet*);

};


inline void
IPRw::Mapping::append_to_free(Mapping*& head, Mapping*& tail)
{
    assert((!head && !tail)
	   || (head && tail && (head->_flags & F_FREE_TRACKED) && (tail->_flags & F_FREE_TRACKED)));
    assert(!_free_next && !_reverse->_free_next);
    if (tail)
	tail = tail->_free_next = this;
    else
	head = tail = this;
}

inline void
IPRw::Mapping::add_to_free_tracked_tail(Mapping*& head, Mapping*& tail)
{
    assert(!(_flags & F_FREE_TRACKED) && !(_reverse->_flags & F_FREE_TRACKED));
    _flags |= F_FREE_TRACKED;
    _reverse->_flags |= F_FREE_TRACKED;
    primary()->append_to_free(head, tail);
}

inline void
IPRw::Mapping::clear_free_tracked()
{
    _flags &= ~F_FREE_TRACKED;
    _reverse->_flags &= ~F_FREE_TRACKED;
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
