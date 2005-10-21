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
class Timer;
class Element;
class ErrorHandler;
class Bitvector;
class Handler;

/** @file <click/element.hh>
 * @brief Click's Element class.
 */

#define CLICK_ELEMENT_PORT_COUNT_DEPRECATED CLICK_DEPRECATED

// #define CLICK_STATS 5

typedef int (*HandlerHook)(int operation, String&, Element*, const Handler*, ErrorHandler*);
typedef String (*ReadHandlerHook)(Element*, void*);
typedef int (*WriteHandlerHook)(const String&, Element*, void*, ErrorHandler*);

class Element { public:
    
    Element();
    virtual ~Element();
    static int nelements_allocated;

    // RUNTIME
    virtual void push(int port, Packet*);
    virtual Packet* pull(int port);
    virtual Packet* simple_action(Packet*);

    virtual bool run_task();		// return true iff did useful work
    virtual void run_timer(Timer *);
#if CLICK_USERLEVEL
    virtual void selected(int fd);
#endif

    inline void checked_output_push(int port, Packet*) const;
    
    // ELEMENT CHARACTERISTICS
    virtual const char *class_name() const = 0;

    virtual const char *port_count() const;
    static const char PORTS_0_0[];
    static const char PORTS_0_1[];
    static const char PORTS_1_0[];
    static const char PORTS_1_1[];
    
    virtual const char *processing() const;
    static const char AGNOSTIC[];
    static const char PUSH[];
    static const char PULL[];
    static const char PUSH_TO_PULL[];
    static const char PULL_TO_PUSH[];
    
    virtual const char *flow_code() const;
    static const char COMPLETE_FLOW[];
    
    virtual const char *flags() const;

    virtual void *cast(const char *);
    
    // CONFIGURATION, INITIALIZATION, AND CLEANUP
    enum ConfigurePhase {
	CONFIGURE_PHASE_FIRST = 0,
	CONFIGURE_PHASE_INFO = 20,
	CONFIGURE_PHASE_PRIVILEGED = 90,
	CONFIGURE_PHASE_DEFAULT = 100,
	CONFIGURE_PHASE_LAST = 2000
    };
    virtual int configure_phase() const;
    
    virtual int configure(Vector<String>&, ErrorHandler*);
    
    virtual void add_handlers();
  
    virtual int initialize(ErrorHandler*);
    
    virtual void take_state(Element *old_element, ErrorHandler*);
    virtual Element *hotswap_element() const;

    enum CleanupStage {
	CLEANUP_NO_ROUTER,
	CLEANUP_CONFIGURE_FAILED,
	CLEANUP_CONFIGURED,
	CLEANUP_INITIALIZE_FAILED,
	CLEANUP_INITIALIZED,
	CLEANUP_ROUTER_INITIALIZED,
	CLEANUP_MANUAL
    };
    virtual void cleanup(CleanupStage);

    static inline void static_initialize();
    static inline void static_cleanup();

    // ELEMENT ROUTER CONNECTIONS
    String id() const;
    String landmark() const;
    virtual String declaration() const;
  
    inline Router *router() const;
    inline int eindex() const;
    inline int eindex(Router *r) const;
    Master *master() const;

    // INPUTS AND OUTPUTS
    inline int nports(bool isoutput) const;
    inline int ninputs() const;
    inline int noutputs() const;

    class Port;
    inline const Port &port(bool isoutput, int port) const;
    inline const Port &input(int port) const;
    inline const Port &output(int port) const;

    inline bool port_active(bool isoutput, int port) const;
    inline bool input_is_push(int port) const;
    inline bool input_is_pull(int port) const;
    inline bool output_is_push(int port) const;
    inline bool output_is_pull(int port) const;
    void port_flow(bool isoutput, int port, Bitvector*) const;
  
    // LIVE RECONFIGURATION
    virtual void configuration(Vector<String>&) const;
    String configuration() const;
  
    virtual bool can_live_reconfigure() const;
    virtual int live_reconfigure(Vector<String>&, ErrorHandler*);

#if CLICK_USERLEVEL
    // SELECT
    enum { SELECT_READ = 1, SELECT_WRITE = 2 };
    int add_select(int fd, int mask);
    int remove_select(int fd, int mask);
#endif

    // HANDLERS
    void add_read_handler(const String &name, ReadHandlerHook, void*);
    void add_write_handler(const String &name, WriteHandlerHook, void*);
    void set_handler(const String &name, int flags, HandlerHook, void* = 0, void* = 0);
    void add_task_handlers(Task*, const String& prefix = String());

    static String read_positional_handler(Element*, void*);
    static String read_keyword_handler(Element*, void*);
    static int reconfigure_positional_handler(const String&, Element*, void*, ErrorHandler*);
    static int reconfigure_keyword_handler(const String&, Element*, void*, ErrorHandler*);
  
    virtual int llrpc(unsigned command, void* arg);
    int local_llrpc(unsigned command, void* arg);

#if CLICK_STATS >= 2
    // STATISTICS
    int _calls; // Push and pull calls into this element.
    uint64_t _self_cycles;  // Cycles spent in self and children.
    uint64_t _child_cycles; // Cycles spent in children.
#endif
  
    class Port { public:
    
	inline bool active() const;
	inline Element* element() const;
	inline int port() const;
    
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

	inline Port();
	inline Port(Element*, Element*, int);
	
	friend class Element;
    
    };

    // DEPRECATED
    Element(int ninputs, int noutputs) CLICK_ELEMENT_PORT_COUNT_DEPRECATED;

    virtual void notify_ninputs(int) CLICK_ELEMENT_PORT_COUNT_DEPRECATED;
    virtual void notify_noutputs(int) CLICK_ELEMENT_PORT_COUNT_DEPRECATED;

    inline void set_ninputs(int) CLICK_ELEMENT_PORT_COUNT_DEPRECATED;
    inline void set_noutputs(int) CLICK_ELEMENT_PORT_COUNT_DEPRECATED;
    inline void add_input() CLICK_ELEMENT_PORT_COUNT_DEPRECATED;
    inline void add_output() CLICK_ELEMENT_PORT_COUNT_DEPRECATED;
    bool ports_frozen() const CLICK_DEPRECATED;
    
    virtual void run_timer() CLICK_DEPRECATED;
    
  private:

    enum { INLINE_PORTS = 4 };

    Port* _ports[2];
    Port _inline_ports[INLINE_PORTS];

    int _nports[2];

    Router* _router;
    int _eindex;

    Element(const Element &);
    Element &operator=(const Element &);
  
    // METHODS USED BY ROUTER
    inline void attach_router(Router* r, int n)	{ _router = r; _eindex = n; }
    
    int set_nports(int, int);
    int notify_nports(int, int, ErrorHandler *);
    enum Processing { VAGNOSTIC, VPUSH, VPULL };
    static int next_processing_code(const char*& p, ErrorHandler* errh);
    void processing_vector(int* input_codes, int* output_codes, ErrorHandler*) const;

    void initialize_ports(const int* input_codes, const int* output_codes);
    int connect_port(bool isoutput, int port, Element*, int);
    
    void add_default_handlers(bool writable_config);

    friend class Router;
    
};


/** @brief Initialize static data for this element class.
 *
 * Elements that need to initialize global state, such as global hash tables
 * or configuration parsing functions, should place that initialization code
 * inside a static_initialize() static member function.  Click's build
 * machinery will find that function and cause it to be called when the
 * element code is loaded, before any elements of the class are created.
 *
 * static_initialize functions are called in an arbitrary and unpredictable
 * order (not, for example, the configure_phase() order).  Element authors are
 * responsible for handling static initialization dependencies.
 *
 * For Click to find a static_initialize declaration, it must appear inside
 * the element class's class declaration on its own line and have the
 * following prototype:
 *
 * @code
 * static void static_initialize();
 * @endcode
 *
 * It must also have public accessibility.
 *
 * @sa Element::static_cleanup
 */
inline void
Element::static_initialize()
{
}

/** @brief Clean up static data for this element class.
 *
 * Elements that need to free global state, such as global hash tables or
 * configuration parsing functions, should place that code inside a
 * static_cleanup() static member function.  Click's build machinery will find
 * that function and cause it to be called when the element code is unloaded.
 *
 * static_cleanup functions are called in an arbitrary and unpredictable order
 * (not, for example, the configure_phase() order, and not the reverse of the
 * static_initialize order).  Element authors are responsible for handling
 * static cleanup dependencies.
 *
 * For Click to find a static_cleanup declaration, it must appear inside the
 * element class's class declaration on its own line and have the following
 * prototype:
 *
 * @code
 * static void static_cleanup();
 * @endcode
 *
 * It must also have public accessibility.
 *
 * @sa Element::static_initialize
 */
inline void
Element::static_cleanup()
{
}

/** @brief Return the element's router. */
inline Router*
Element::router() const
{
    return _router;
}

/** @brief Return the element's index within its router.
 * @invariant this == router()->element(eindex())
 */
inline int
Element::eindex() const
{
    return _eindex;
}

/** @brief Return the element's index within router @a r.
 * 
 * Returns -1 if @a r != router(). */
inline int
Element::eindex(Router* r) const
{
    return (router() == r ? _eindex : -1);
}

/** @brief Return the number of input or output ports.
 * @param isoutput false for input ports, true for output ports */
inline int
Element::nports(bool isoutput) const
{
    return _nports[isoutput];
}

/** @brief Return the number of input ports. */
inline int
Element::ninputs() const
{
    return _nports[0];
}

/** @brief Return the number of output ports. */
inline int
Element::noutputs() const
{
    return _nports[1];
}

/** @brief Sets the element's number of input ports (deprecated).
 *
 * @param ninputs number of input ports
 *
 * @deprecated The set_ninputs() function is deprecated.  Elements should
 * instead use port_count() to define an acceptable range of input port
 * counts.  Elements that called set_ninputs() from configure(), setting the
 * number of input ports based on configuration arguments, should compare the
 * desired number of ports to ninputs() and signal an error if they disagree.
 *
 * This function can be called from the constructor, notify_ninputs(),
 * notify_noutputs(), or configure(), but not from initialize() or later.  */
inline void
Element::set_ninputs(int ninputs)
{
    set_nports(ninputs, _nports[1]);
}

/** @brief Sets the element's number of output ports (deprecated).
 *
 * @param noutputs number of output ports
 *
 * @deprecated The set_noutputs() function is deprecated.  Elements should
 * instead use port_count() to define an acceptable range of output port
 * counts.  Elements that called set_noutputs() from configure(), setting the
 * number of output ports based on configuration arguments, should compare the
 * desired number of ports to noutputs() and signal an error if they disagree.
 *
 * This function can be called from the constructor, notify_ninputs(),
 * notify_noutputs(), or configure(), but not from initialize() or later.  */
inline void
Element::set_noutputs(int noutputs)
{
    set_nports(_nports[0], noutputs);
}

/** @brief Adds an input port (deprecated).
 *
 * @deprecated See the deprecation note at set_ninputs().
 *
 * An abbreviation for set_ninputs(ninputs() + 1).  Subject to the same
 * restrictions as set_ninputs().
 *
 * @sa set_ninputs */
inline void
Element::add_input()
{
    set_nports(_nports[0] + 1, _nports[1]);
}

/** @brief Adds an output port (deprecated).
 *
 * @deprecated See the deprecation note at set_noutputs().
 *
 * An abbreviation for set_noutputs(noutputs() + 1).  Subject to the same
 * restrictions as set_noutputs().
 *
 * @sa set_noutputs */
inline void
Element::add_output()
{
    set_nports(_nports[0], _nports[1] + 1);
}

/** @brief Return one of the element's ports.
 * @param isoutput false for input ports, true for output ports
 * @param port port number
 *
 * An assertion fails if @a p is out of range. */
inline const Element::Port&
Element::port(bool isoutput, int port) const
{
    assert((unsigned) port < (unsigned) _nports[isoutput]);
    return _ports[isoutput][port];
}

/** @brief Return one of the element's input ports.
 * @param port port number
 *
 * An assertion fails if @a port is out of range.
 *
 * @sa Port, port */
inline const Element::Port&
Element::input(int port) const
{
    return Element::port(false, port);
}

/** @brief Return one of the element's output ports.
 * @param port port number
 *
 * An assertion fails if @a port is out of range.
 *
 * @sa Port, port */
inline const Element::Port&
Element::output(int port) const
{
    return Element::port(true, port);
}

/** @brief Check whether a port is active.
 * @param isoutput false for input ports, true for output ports
 * @param port port number
 *
 * Returns true iff @a port is in range and @a port is active.  Push outputs
 * and pull inputs are active; pull outputs and push inputs are not.
 *
 * @sa Element::Port::active */
inline bool
Element::port_active(bool isoutput, int port) const
{
    return (unsigned) port < (unsigned) nports(isoutput)
	&& _ports[isoutput][port].active();
}

/** @brief Check whether output @a port is push.
 *
 * Returns true iff output @a port exists and is push.  @sa port_active */
inline bool
Element::output_is_push(int port) const
{
    return port_active(true, port);
}

/** @brief Check whether output @a port is pull.
 *
 * Returns true iff output @a port exists and is pull. */
inline bool
Element::output_is_pull(int port) const
{
    return (unsigned) port < (unsigned) nports(true)
	&& !_ports[1][port].active();
}

/** @brief Check whether input @a port is pull.
 *
 * Returns true iff input @a port exists and is pull.  @sa port_active */
inline bool
Element::input_is_pull(int port) const
{
    return port_active(false, port);
}

/** @brief Check whether input @a port is push.
 *
 * Returns true iff input @a port exists and is push. */
inline bool
Element::input_is_push(int port) const
{
    return (unsigned) port < (unsigned) nports(false)
	&& !_ports[0][port].active();
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

/** @brief Returns whether this port is active (a push output or a pull input).
 *
 * @sa Element::port_active
 */
inline bool
Element::Port::active() const
{
    return _port >= 0;
}
    
/** @brief Returns the element connected to this active port.
 *
 * Returns 0 if this port is not active(). */
inline Element*
Element::Port::element() const
{
    return _e;
}

/** @brief Returns the port number of the port connected to this active port.
 *
 * Returns < 0 if this port is not active(). */
inline int
Element::Port::port() const
{
    return _port;
}

/** @brief Push packet @a p over this port.
 *
 * Pushes packet @a p downstream through the router configuration by passing
 * it to the next element's @link Element::push() push() @endlink function.
 * Returns when the rest of the router finishes processing @a p.
 *
 * This port must be an active() push output port.  Usually called from
 * element code like @link Element::output output(i) @endlink .push(p).
 *
 * When element code calls Element::Port::push(@a p), it relinquishes control
 * of packet @a p.  When push() returns, @a p may have been altered or even
 * freed by downstream elements.  Thus, you must not use @a p after pushing it
 * downstream.  To push a copy and keep a copy, see Packet::clone().
 *
 * output(i).push(p) basically behaves like the following code, although it
 * maintains additional statistics depending on how CLICK_STATS is defined:
 *
 * @code
 * output(i).element()->push(output(i).port(), p);
 * @endcode
 */
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

/** @brief Pull a packet over this port and return it.
 *
 * Pulls a packet from upstream in the router configuration by calling the
 * previous element's @link Element::pull() pull() @endlink function.  When
 * the router finishes processing, returns the result.
 *
 * This port must be an active() pull input port.  Usually called from element
 * code like @link Element::input input(i) @endlink .pull().
 *
 * input(i).pull() basically behaves like the following code, although it
 * maintains additional statistics depending on how CLICK_STATS is defined:
 *
 * @code
 * input(i).element()->pull(input(i).port())
 * @endcode
 */
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

/** @brief Push packet @a p to output @a port, or kill it if @a port is out of
 * range.
 *
 * @param port output port number
 * @param p packet to push
 *
 * If @a port is in range (>= 0 and < noutputs()), then push packet @a p
 * forward using output(@a port).push(@a p).  Otherwise, kill @a p with @a p
 * ->kill().
 *
 * @note It is invalid to call checked_output_push() on a pull output @a port.
 */
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
