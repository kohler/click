// -*- c-basic-offset: 4; related-file-name: "../../lib/router.cc" -*-
#ifndef CLICK_ROUTER_HH
#define CLICK_ROUTER_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/sync.hh>
#include <click/task.hh>
#include <click/standard/threadsched.hh>
#if CLICK_NS
# include <click/simclick.h>
#endif
CLICK_DECLS
class Master;
class ElementFilter;
class RouterThread;
class HashMap_ArenaFactory;
class NotifierSignal;
class ThreadSched;
class Handler;

class Router { public:

    struct Hookup {
	int idx;
	int port;
	Hookup()				: idx(-1) { }
	Hookup(int i, int p)			: idx(i), port(p) { }
    };
  
    Router(const String &configuration, Master *);
    ~Router();
    void use()					{ _refcount++; }
    void unuse();

    static void static_initialize();
    static void static_cleanup();

    enum {
	ROUTER_NEW, ROUTER_PRECONFIGURE, ROUTER_PREINITIALIZE,
	ROUTER_LIVE, ROUTER_DEAD		// order is important
    };
    bool initialized() const			{ return _state == ROUTER_LIVE; }
    bool handlers_ready() const			{ return _state > ROUTER_PRECONFIGURE; }

    // ELEMENTS
    int nelements() const			{ return _elements.size(); }
    const Vector<Element*>& elements() const	{ return _elements; }
    static Element* element(const Router*, int);
    Element* element(int e) const		{ return element(this, e); }
    // element() returns 0 on bad index/no router, root_element() on index -1
    Element* root_element() const		{ return _root_element; }
  
    const String& ename(int) const;
    const String& elandmark(int) const;
    const String& default_configuration_string(int) const;
    void set_default_configuration_string(int, const String&);
  
    Element* find(const String&, ErrorHandler* = 0) const;
    Element* find(const String&, Element* context, ErrorHandler* = 0) const;
    Element* find(const String&, String prefix, ErrorHandler* = 0) const;
  
    int downstream_elements(Element*, int o, ElementFilter*, Vector<Element*>&);
    int downstream_elements(Element*, int o, Vector<Element*> &);
    int downstream_elements(Element*, Vector<Element*> &);
    int upstream_elements(Element*, int i, ElementFilter*, Vector<Element*>&);
    int upstream_elements(Element*, int i, Vector<Element*> &);  
    int upstream_elements(Element*, Vector<Element*> &);

    // HANDLERS
    enum { FIRST_GLOBAL_HANDLER = 0x40000000 };
    static int hindex(const Element*, const String&);
    static void element_hindexes(const Element*, Vector<int>&);

    // 'const Handler*' results last until that element/handlername modified
    static const Handler* handler(const Router*, int);
    static const Handler* handler(const Element*, int);
    const Handler* handler(int) const;
    static const Handler* handler(const Element*, const String&);

    static void add_read_handler(const Element*, const String&, ReadHandler, void*);
    static void add_write_handler(const Element*, const String&, WriteHandler, void*);
    static void set_handler(const Element*, const String&, int mask, HandlerHook, void* = 0, void* = 0);
    static int change_handler_flags(const Element*, const String&, uint32_t clear_flags, uint32_t set_flags);

    // ATTACHMENTS AND REQUIREMENTS
    void* attachment(const String&) const;
    void*& force_attachment(const String&);
    void* set_attachment(const String&, void*);
    const Vector<String>& requirements() const	{ return _requirements; }

    Router* hotswap_router() const		{ return _hotswap_router; }
    void set_hotswap_router(Router*);
    
    ErrorHandler* chatter_channel(const String&) const;
    HashMap_ArenaFactory* arena_factory() const;

    ThreadSched* thread_sched() const		{ return _thread_sched; }
    void set_thread_sched(ThreadSched* ts)	{ _thread_sched = ts; }
    int initial_thread_preference(Task*, bool scheduled) const;

    int new_notifier_signal(NotifierSignal&);

    // MASTER
    Master* master() const			{ return _master; }
    enum { RUNNING_DEAD = -2, RUNNING_INACTIVE = -1, RUNNING_PAUSED = 0, RUNNING_BACKGROUND = 1, RUNNING_ACTIVE = 2 };
  
    // RUNCOUNT AND RUNCLASS
    void please_stop_driver()			{ adjust_runcount(-1); }
    void reserve_driver()			{ adjust_runcount(1); }
    void set_runcount(int);
    void adjust_runcount(int);
    int runcount() const			{ return _runcount; }

    // UNPARSING
    const String& configuration_string() const	{ return _configuration; }
    void unparse(StringAccum&, const String& = String()) const;
    void unparse_requirements(StringAccum&, const String& = String()) const;
    void unparse_classes(StringAccum&, const String& = String()) const;
    void unparse_declarations(StringAccum&, const String& = String()) const;
    void unparse_connections(StringAccum&, const String& = String()) const;
  
    String element_ports_string(int) const;
  
    // INITIALIZATION
    void add_requirement(const String&);
    int add_element(Element*, const String& name, const String& conf, const String& landmark);
    int add_connection(int from_idx, int from_port, int to_idx, int to_port);
#if CLICK_LINUXMODULE
    int add_module_ref(struct module *);
#endif
  
    int initialize(ErrorHandler*);
    void activate(bool foreground, ErrorHandler*);
    void activate(ErrorHandler* errh)		{ activate(true, errh); }

#if CLICK_NS
    int sim_get_ifid(const char* ifname);
    int sim_listen(int ifid, int element);
    int sim_if_ready(int ifid);
    int sim_write(int ifid, int ptype, const unsigned char *, int len,
		  simclick_simpacketinfo *pinfo);
    int sim_incoming_packet(int ifid, int ptype, const unsigned char *,
			    int len, simclick_simpacketinfo* pinfo);

  protected:
    Vector<Vector<int> *> _listenvecs;
    Vector<int>* sim_listenvec(int ifid);
#endif
  
  private:

    Master* _master;
    
    volatile int _runcount;

    atomic_uint32_t _refcount;
  
    Vector<Element*> _elements;
    Vector<String> _element_names;
    Vector<String> _element_configurations;
    Vector<String> _element_landmarks;
    Vector<int> _element_configure_order;

    Vector<Hookup> _hookup_from;
    Vector<Hookup> _hookup_to;

    struct Gport {
	Vector<int> e2g;
	Vector<int> g2e;
	int size() const			{ return g2e.size(); }
    };
    Gport _gports[2];
  
    Vector<int> _hookup_gports[2];

    Vector<String> _requirements;

    int _state;
    mutable bool _allow_star_handler : 1;
    int _running;
  
    Vector<int> _ehandler_first_by_element;
    Vector<int> _ehandler_to_handler;
    Vector<int> _ehandler_next;

    Vector<int> _handler_first_by_name;

    enum { HANDLER_BUFSIZ = 256 };
    Handler** _handler_bufs;
    int _nhandlers_bufs;
    int _free_handler;

    Vector<String> _attachment_names;
    Vector<void*> _attachments;
  
    Element* _root_element;
    String _configuration;

    enum { NOTIFIER_SIGNALS_CAPACITY = 4096 };
    atomic_uint32_t* _notifier_signals;
    int _n_notifier_signals;
    HashMap_ArenaFactory* _arena_factory;
    Router* _hotswap_router;
    ThreadSched* _thread_sched;

    Router* _next_router;

#if CLICK_LINUXMODULE
    Vector<struct module*> _modules;
#endif
    
    Router(const Router&);
    Router& operator=(const Router&);
  
    void remove_hookup(int);
    void hookup_error(const Hookup&, bool, const char*, ErrorHandler*);
    int check_hookup_elements(ErrorHandler*);
    void notify_hookup_range();
    void check_hookup_range(ErrorHandler*);
    void check_hookup_completeness(ErrorHandler*);  
  
    int processing_error(const Hookup&, const Hookup&, bool, int, ErrorHandler*);
    int check_push_and_pull(ErrorHandler*);
    
    void make_gports();
    int ngports(bool isout) const	{ return _gports[isout].g2e.size(); }
    inline int gport(bool isoutput, const Hookup&) const;
    inline Hookup gport_hookup(bool isoutput, int) const;
    void gport_list_elements(bool, const Bitvector&, Vector<Element*>&) const;
    void make_hookup_gports();
  
    void set_connections();
  
    String context_message(int element_no, const char*) const;
    int element_lerror(ErrorHandler*, Element*, const char*, ...) const;

    // private handler methods
    void initialize_handlers(bool, bool);
    inline Handler* xhandler(int) const;
    int find_ehandler(int, const String&) const;
    static inline Handler fetch_handler(const Element*, const String&);
    void store_local_handler(int, const Handler&);
    static void store_global_handler(const Handler&);
    static inline void store_handler(const Element*, const Handler&);

    int global_port_flow(bool forward, Element* first_element, int first_port, ElementFilter* stop_filter, Bitvector& results);

    // global handlers
    static String router_read_handler(Element*, void*);

    friend class Master;
    friend class Task;
  
};


class Handler { public:

    enum {
	OP_READ = 1, OP_WRITE = 2, READ_PARAM = 4, ONE_HOOK = 8,
	PRIVATE_MASK = 0xF, 
	DRIVER_FLAG_0 = 16, DRIVER_FLAG_1 = 32, DRIVER_FLAG_2 = 64,
	DRIVER_FLAG_3 = 128,
	USER_FLAG_SHIFT = 8, USER_FLAG_0 = 1 << USER_FLAG_SHIFT
    };

    const String& name() const	{ return _name; }
    uint32_t flags() const	{ return _flags; }
    void* thunk() const		{ return _thunk; }
    void* thunk2() const	{ return _thunk2; }

    bool readable() const	{ return _flags & OP_READ; }
    bool read_param() const	{ return _flags & READ_PARAM; }
    bool read_visible() const	{ return _flags & OP_READ; }
    bool writable() const	{ return _flags & OP_WRITE; }
    bool write_visible() const	{ return _flags & OP_WRITE; }
    bool visible() const	{ return _flags & (OP_READ | OP_WRITE); }

    inline String call_read(Element*, ErrorHandler* = 0) const;
    String call_read(Element*, const String&, ErrorHandler* = 0) const;
    int call_write(const String&, Element*, ErrorHandler*) const;
  
    String unparse_name(Element*) const;
    static String unparse_name(Element*, const String&);

    static const Handler* blank_handler() { return the_blank_handler; }
    
  private:
  
    String _name;
    union {
	HandlerHook h;
	struct {
	    ReadHandler r;
	    WriteHandler w;
	} rw;
    } _hook;
    void* _thunk;
    void* _thunk2;
    uint32_t _flags;
    int _use_count;
    int _next_by_name;

    static const Handler* the_blank_handler;
    
    Handler(const String& = String());

    bool compatible(const Handler&) const;
  
    friend class Router;
  
};

/* The largest size a write handler is allowed to have. */
#define LARGEST_HANDLER_WRITE 65536


inline bool
operator==(const Router::Hookup& a, const Router::Hookup& b)
{
    return a.idx == b.idx && a.port == b.port;
}

inline bool
operator!=(const Router::Hookup& a, const Router::Hookup& b)
{
    return a.idx != b.idx || a.port != b.port;
}

inline Element *
Router::find(const String& name, ErrorHandler *errh) const
{
    return find(name, "", errh);
}

inline const Handler*
Router::handler(const Element* e, int hi)
{
    return handler(e ? e->router() : 0, hi);
}

inline const Handler*
Router::handler(int hi) const
{
    return handler(this, hi);
}

inline
Handler::Handler(const String &name)
    : _name(name), _thunk(0), _thunk2(0), _flags(0), _use_count(0),
      _next_by_name(-1)
{
    _hook.rw.r = 0;
    _hook.rw.w = 0;
}

inline bool
Handler::compatible(const Handler& o) const
{
    return (_hook.rw.r == o._hook.rw.r && _hook.rw.w == o._hook.rw.w
	    && _thunk == o._thunk && _thunk2 == o._thunk2
	    && _flags == o._flags);
}

inline String
Handler::call_read(Element* e, ErrorHandler* errh) const
{
    return call_read(e, String(), errh);
}

inline HashMap_ArenaFactory*
Router::arena_factory() const
{
    return _arena_factory;
}

inline int
Router::initial_thread_preference(Task* t, bool scheduled) const
{
    if (!_thread_sched)
	return ThreadSched::THREAD_PREFERENCE_UNKNOWN;
    else
	return _thread_sched->initial_thread_preference(t, scheduled);
}

CLICK_ENDDECLS
#endif
