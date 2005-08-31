// -*- c-basic-offset: 4; related-file-name: "../../lib/element.cc" -*-
#ifndef CLICK_ELEMENT_HH
#define CLICK_ELEMENT_HH
#include <click/glue.hh>
#include <click/vector.hh>
#include <click/string.hh>
#include <click/packet.hh>
CLICK_DECLS
class Router;
class Master;
class Task;
class Element;
class ErrorHandler;
class Bitvector;
class Handler;

// #define CLICK_STATS 5

typedef int (*HandlerHook)(int operation, String&, Element*, const Handler*, ErrorHandler*);
typedef String (*ReadHandler)(Element*, void*);
typedef int (*WriteHandler)(const String&, Element*, void*, ErrorHandler*);

class Element { public:
    
    Element();
    Element(int ninputs, int noutputs);
    virtual ~Element();
    static int nelements_allocated;

    static void static_initialize()	{ }
    static void static_cleanup()	{ }

    // RUNTIME
    virtual void push(int port, Packet*);
    virtual Packet* pull(int port);
    virtual Packet* simple_action(Packet*);

    virtual bool run_task();		// return true iff did useful work
    virtual void run_timer();
#if CLICK_USERLEVEL
    virtual void selected(int fd);
#endif
    virtual void run_scheduled();	// deprecated, use run_{task,timer}()
  
    // CHARACTERISTICS
    virtual const char* class_name() const = 0;
    virtual void* cast(const char*);
  
    String id() const;
    String declaration() const;
    String landmark() const;
  
    Router* router() const			{ return _router; }
    Master* master() const;
    int eindex() const				{ return _eindex; }
    inline int eindex(Router*) const;

    // INPUTS AND OUTPUTS
    inline int nports(bool isoutput) const	{ return _nports[isoutput]; }
    int ninputs() const				{ return _nports[0]; }
    int noutputs() const			{ return _nports[1]; }
    void set_ninputs(int);
    void set_noutputs(int);
    void add_input()				{ set_ninputs(ninputs()+1); }
    void add_output()				{ set_noutputs(noutputs()+1); }
    bool ports_frozen() const;
  
    class Port;
    inline const Port& port(bool isoutput, int) const;
    inline const Port& input(int) const;
    inline const Port& output(int) const;

    inline bool port_allowed(bool isoutput, int) const;
    inline bool input_is_push(int) const;
    inline bool input_is_pull(int) const;
    inline bool output_is_push(int) const;
    inline bool output_is_pull(int) const;
  
    inline void checked_output_push(int, Packet*) const;

    // PROCESSING, FLOW, AND FLAGS
    virtual const char* processing() const;
    static const char AGNOSTIC[];
    static const char PUSH[];
    static const char PULL[];
    static const char PUSH_TO_PULL[];
    static const char PULL_TO_PUSH[];
    
    virtual const char* flow_code() const;
    static const char COMPLETE_FLOW[];
    
    virtual const char* flags() const;
  
    // CONFIGURATION AND INITIALIZATION
    virtual void notify_ninputs(int);
    virtual void notify_noutputs(int);

    enum ConfigurePhase {
	CONFIGURE_PHASE_FIRST = 0, CONFIGURE_PHASE_INFO = 20,
	CONFIGURE_PHASE_PRIVILEGED = 90, CONFIGURE_PHASE_DEFAULT = 100,
	CONFIGURE_PHASE_LAST = 2000
    };
    virtual int configure_phase() const;
    virtual int configure(Vector<String>&, ErrorHandler*);
    virtual int initialize(ErrorHandler*);

    // CLEANUP
    enum CleanupStage {
	CLEANUP_NO_ROUTER, CLEANUP_CONFIGURE_FAILED, CLEANUP_CONFIGURED,
	CLEANUP_INITIALIZE_FAILED, CLEANUP_INITIALIZED,
	CLEANUP_ROUTER_INITIALIZED, CLEANUP_MANUAL
    };
    virtual void cleanup(CleanupStage);

    // LIVE RECONFIGURATION, HOTSWAP
    virtual void configuration(Vector<String>&) const;
    String configuration() const;
  
    virtual bool can_live_reconfigure() const;
    virtual int live_reconfigure(Vector<String>&, ErrorHandler*);

    virtual Element* hotswap_element() const;
    virtual void take_state(Element*, ErrorHandler*);

    // HANDLERS
    virtual void add_handlers();
  
    void add_read_handler(const String&, ReadHandler, void*);
    void add_write_handler(const String&, WriteHandler, void*);
    void set_handler(const String&, int flags, HandlerHook, void* = 0, void* = 0);
    void add_task_handlers(Task*, const String& prefix = String());
  
    static String read_positional_handler(Element*, void*);
    static String read_keyword_handler(Element*, void*);
    static int reconfigure_positional_handler(const String&, Element*, void*, ErrorHandler*);
    static int reconfigure_keyword_handler(const String&, Element*, void*, ErrorHandler*);
  
    virtual int llrpc(unsigned command, void* arg);
    int local_llrpc(unsigned command, void* arg);
  
#if CLICK_USERLEVEL
    // SELECT
    enum { SELECT_READ = 1, SELECT_WRITE = 2 };
    int add_select(int fd, int mask);
    int remove_select(int fd, int mask);
#endif

    // METHODS USED BY ROUTER
    void attach_router(Router* r, int n)	{ _router = r; _eindex = n; }
    
    enum Processing { VAGNOSTIC, VPUSH, VPULL };
    void processing_vector(int* input_codes, int* output_codes, ErrorHandler*) const;
    void port_flow(bool isoutput, int, Bitvector*) const;

    void initialize_ports(const int* input_codes, const int* output_codes);
    int connect_port(bool isoutput, int port, Element*, int);
    
    void add_default_handlers(bool writable_config);

#if CLICK_STATS >= 2
    // STATISTICS
    int _calls; // Push and pull calls into this element.
    uint64_t _self_cycles;  // Cycles spent in self and children.
    uint64_t _child_cycles; // Cycles spent in children.
#endif
  
    class Port { public:

	inline Port();
	inline Port(Element*, Element*, int);
    
	operator bool() const		{ return _e != 0; }
	bool allowed() const		{ return _port >= 0; }
	bool initialized() const	{ return _port >= -1; }
    
	Element* element() const	{ return _e; }
	int port() const		{ return _port; }
    
	inline void push(Packet* p) const;
	inline Packet* pull() const;

#if CLICK_STATS >= 1
	unsigned npackets() const	{ return _packets; }
#endif

      private:
    
	Element* _e;
	int _port;
    
#if CLICK_STATS >= 1
	mutable unsigned _packets;	// How many packets have we moved?
#endif
#if CLICK_STATS >= 2
	Element* _owner;		// Whose input or output are we?
#endif
    
    };

  private:

    enum { INLINE_PORTS = 4 };

    Port* _ports[2];
    Port _inline_ports[INLINE_PORTS];

    int _nports[2];

    Router* _router;
    int _eindex;

    Element(const Element &);
    Element &operator=(const Element &);
  
    void set_nports(int, int);
  
};


inline int
Element::eindex(Router* r) const
{
    return (router() == r ? eindex() : -1);
}

inline const Element::Port&
Element::port(bool isoutput, int p) const
{
    assert((unsigned) p < (unsigned) _nports[isoutput]);
    return _ports[isoutput][p];
}

inline const Element::Port&
Element::input(int p) const
{
    return port(false, p);
}

inline const Element::Port&
Element::output(int p) const
{
    return port(true, p);
}

inline bool
Element::port_allowed(bool isoutput, int p) const
{
    return (unsigned) p < (unsigned) nports(isoutput) && _ports[isoutput][p].allowed();
}

inline bool
Element::output_is_push(int p) const
{
    return port_allowed(true, p);
}

inline bool
Element::output_is_pull(int p) const
{
    return (unsigned) p < (unsigned) nports(true) && !_ports[1][p].allowed();
}

inline bool
Element::input_is_pull(int p) const
{
    return port_allowed(false, p);
}

inline bool
Element::input_is_push(int p) const
{
    return (unsigned) p < (unsigned) nports(false) && !_ports[0][p].allowed();
}

#if CLICK_STATS >= 2
# define PORT_CTOR_INIT(o) , _packets(0), _owner(o)
#else
# if CLICK_STATS >= 1
#  define PORT_CTOR_INIT(o) , _packets(0)
# else
#  define PORT_CTOR_INIT(o)
# endif
#endif

inline
Element::Port::Port()
    : _e(0), _port(-2) PORT_CTOR_INIT(0)
{
}

inline
Element::Port::Port(Element* owner, Element* e, int p)
    : _e(e), _port(p) PORT_CTOR_INIT(owner)
{
    (void) owner;
}

inline void
Element::Port::push(Packet* p) const
{
    assert(_e);
#if CLICK_STATS >= 1
    _packets++;
#endif
#if CLICK_STATS >= 2
    _e->input(_port)._packets++;
    uint64_t c0 = click_get_cycles();
    _e->push(_port, p);
    uint64_t c1 = click_get_cycles();
    uint64_t x = c1 - c0;
    _e->_calls += 1;
    _e->_self_cycles += x;
    _owner->_child_cycles += x;
#else
    _e->push(_port, p);
#endif
}

inline Packet*
Element::Port::pull() const
{
    assert(_e);
#if CLICK_STATS >= 2
    _e->output(_port)._packets++;
    uint64_t c0 = click_get_cycles();
    Packet *p = _e->pull(_port);
    uint64_t c1 = click_get_cycles();
    uint64_t x = c1 - c0;
    _e->_calls += 1;
    _e->_self_cycles += x;
    _owner->_child_cycles += x;
#else
    Packet* p = _e->pull(_port);
#endif
#if CLICK_STATS >= 1
    if (p)
	_packets++;
#endif
    return p;
}

inline void
Element::checked_output_push(int port, Packet* p) const
{
    if ((unsigned) port < (unsigned) noutputs())
	_ports[1][port].push(p);
    else
	p->kill();
}

#undef CONNECTION_CTOR_INIT
CLICK_ENDDECLS
#endif
