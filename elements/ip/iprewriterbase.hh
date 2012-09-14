// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPREWRITERBASE_HH
#define CLICK_IPREWRITERBASE_HH
#include <click/timer.hh>
#include "elements/ip/iprwmapping.hh"
#include <click/bitvector.hh>
CLICK_DECLS
class IPMapper;
class IPRewriterPattern;

class IPRewriterInput { public:
    enum {
	i_drop, i_nochange, i_keep, i_pattern, i_mapper
    };
    IPRewriterBase *owner;
    int owner_input;
    int kind;
    int foutput;
    IPRewriterBase *reply_element;
    int routput;
    uint32_t count;
    uint32_t failures;
    union {
	IPRewriterPattern *pattern;
	IPMapper *mapper;
    } u;

    IPRewriterInput()
	: kind(i_drop), foutput(-1), routput(-1), count(0), failures(0) {
	u.pattern = 0;
    }

    enum {
	mapid_default = 0, mapid_iprewriter_udp = 1
    };

    inline int rewrite_flowid(const IPFlowID &flowid,
			      IPFlowID &rewritten_flowid,
			      Packet *p, int mapid = mapid_default);
};

class IPRewriterHeap { public:

    IPRewriterHeap()
	: _capacity(0x7FFFFFFF), _use_count(1) {
    }
    ~IPRewriterHeap() {
	assert(size() == 0);
    }

    void use() {
	++_use_count;
    }
    void unuse() {
	assert(_use_count > 0);
	if (--_use_count == 0)
	    delete this;
    }

    Vector<IPRewriterFlow *>::size_type size() const {
	return _heaps[0].size() + _heaps[1].size();
    }
    int32_t capacity() const {
	return _capacity;
    }

  private:

    enum {
	h_best_effort = 0, h_guarantee = 1
    };
    Vector<IPRewriterFlow *> _heaps[2];
    int32_t _capacity;
    uint32_t _use_count;

    friend class IPRewriterBase;
    friend class IPRewriterFlow;

};

class IPRewriterBase : public Element { public:

    typedef HashContainer<IPRewriterEntry> Map;
    enum {
	rw_drop = -1, rw_addmap = -2
    };

    IPRewriterBase() CLICK_COLD;
    ~IPRewriterBase() CLICK_COLD;

    enum ConfigurePhase {
	CONFIGURE_PHASE_PATTERNS = CONFIGURE_PHASE_INFO,
	CONFIGURE_PHASE_REWRITER = CONFIGURE_PHASE_DEFAULT,
	CONFIGURE_PHASE_MAPPER = CONFIGURE_PHASE_REWRITER - 1,
	CONFIGURE_PHASE_USER = CONFIGURE_PHASE_REWRITER + 1
    };

    const char *port_count() const	{ return "1-/1-"; }
    const char *processing() const	{ return PUSH; }

    int configure_phase() const		{ return CONFIGURE_PHASE_REWRITER; }
    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void add_rewriter_handlers(bool writable_patterns);
    void cleanup(CleanupStage) CLICK_COLD;

    const IPRewriterHeap *flow_heap() const {
	return _heap;
    }
    IPRewriterBase *reply_element(int input) const {
	return _input_specs[input].reply_element;
    }
    virtual HashContainer<IPRewriterEntry> *get_map(int mapid) {
	return likely(mapid == IPRewriterInput::mapid_default) ? &_map : 0;
    }

    enum {
	get_entry_check = -1, get_entry_reply = -2
    };
    virtual IPRewriterEntry *get_entry(int ip_p, const IPFlowID &flowid,
				       int input);
    virtual IPRewriterEntry *add_flow(int ip_p, const IPFlowID &flowid,
				      const IPFlowID &rewritten_flowid,
				      int input) = 0;
    virtual void destroy_flow(IPRewriterFlow *flow) = 0;
    virtual click_jiffies_t best_effort_expiry(const IPRewriterFlow *flow) {
	return flow->expiry() + _timeouts[0] - _timeouts[1];
    }

    int llrpc(unsigned command, void *data);

  protected:

    Map _map;

    Vector<IPRewriterInput> _input_specs;

    IPRewriterHeap *_heap;
    uint32_t _timeouts[2];
    uint32_t _gc_interval_sec;
    Timer _gc_timer;

    enum {
	default_timeout = 300,	   // 5 minutes
	default_guarantee = 5,	   // 5 seconds
	default_gc_interval = 60 * 15 // 15 minutes
    };

    static uint32_t relevant_timeout(const uint32_t timeouts[2]) {
	return timeouts[1] ? timeouts[1] : timeouts[0];
    }

    IPRewriterEntry *store_flow(IPRewriterFlow *flow, int input,
				Map &map, Map *reply_map_ptr = 0);
    inline void unmap_flow(IPRewriterFlow *flow,
			   Map &map, Map *reply_map_ptr = 0);

    static void gc_timer_hook(Timer *t, void *user_data);

    int parse_input_spec(const String &str, IPRewriterInput &is,
			 int input_number, ErrorHandler *errh);

    enum {			// < 0 because individual patterns are >= 0
	h_nmappings = -1, h_mapping_failures = -2, h_patterns = -3,
	h_size = -4, h_capacity = -5, h_clear = -6
    };
    static String read_handler(Element *e, void *user_data) CLICK_COLD;
    static int write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh) CLICK_COLD;
    static int pattern_write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh) CLICK_COLD;

    friend int IPRewriterInput::rewrite_flowid(const IPFlowID &flowid,
			IPFlowID &rewritten_flowid, Packet *p, int mapid);

  private:

    void shift_heap_best_effort(click_jiffies_t now_j);
    bool shrink_heap_for_new_flow(IPRewriterFlow *flow, click_jiffies_t now_j);
    void shrink_heap(bool clear_all);

    friend class IPRewriterFlow;

};


class IPMapper { public:

    IPMapper()				{ }
    virtual ~IPMapper()			{ }

    virtual void notify_rewriter(IPRewriterBase *user, IPRewriterInput *input,
				 ErrorHandler *errh);
    virtual int rewrite_flowid(IPRewriterInput *input,
			       const IPFlowID &flowid,
			       IPFlowID &rewritten_flowid,
			       Packet *p, int mapid);

};


inline int
IPRewriterInput::rewrite_flowid(const IPFlowID &flowid,
				IPFlowID &rewritten_flowid,
				Packet *p, int mapid)
{
    int i;
    switch (kind) {
    case i_nochange:
	return foutput;
    case i_keep:
	rewritten_flowid = flowid;
	return IPRewriterBase::rw_addmap;
    case i_pattern: {
	HashContainer<IPRewriterEntry> *reply_map;
	if (likely(mapid == mapid_default))
	    reply_map = &reply_element->_map;
	else
	    reply_map = reply_element->get_map(mapid);
	i = u.pattern->rewrite_flowid(flowid, rewritten_flowid, *reply_map);
	goto check_for_failure;
    }
    case i_mapper:
	i = u.mapper->rewrite_flowid(this, flowid, rewritten_flowid, p, mapid);
	goto check_for_failure;
    check_for_failure:
	if (i == IPRewriterBase::rw_drop)
	    ++failures;
	return i;
    default:
	return IPRewriterBase::rw_drop;
    }
}

inline void
IPRewriterBase::unmap_flow(IPRewriterFlow *flow, Map &map,
			   Map *reply_map_ptr)
{
    //click_chatter("kill %s", hashkey().s().c_str());
    if (!reply_map_ptr)
	reply_map_ptr = &flow->owner()->reply_element->_map;
    Map::iterator it = map.find(flow->entry(0).hashkey());
    if (it.get() == &flow->entry(0))
	map.erase(it);
    it = reply_map_ptr->find(flow->entry(1).hashkey());
    if (it.get() == &flow->entry(1))
	reply_map_ptr->erase(it);
}

CLICK_ENDDECLS
#endif
