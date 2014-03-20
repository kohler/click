// -*- c-basic-offset: 4; related-file-name: "../../lib/element.cc" -*-
#ifndef CLICK_ELEMENT_HH
#define CLICK_ELEMENT_HH
#include <click/glue.hh>
#include <click/vector.hh>
#include <click/string.hh>
#include <click/packet.hh>
#include <click/handler.hh>
CLICK_DECLS
class Router;
class Master;
class RouterThread;
class Task;
class Timer;
class NotifierSignal;
class Element;
class ErrorHandler;
class Bitvector;
class EtherAddress;

/** @file <click/element.hh>
 * @brief Click's Element class.
 */

#ifndef CLICK_ELEMENT_DEPRECATED
# define CLICK_ELEMENT_DEPRECATED CLICK_DEPRECATED
#endif

class Element { public:

    Element();
    virtual ~Element();
    static int nelements_allocated;

    // RUNTIME
    virtual void push(int port, Packet *p);
    virtual Packet *pull(int port) CLICK_WARN_UNUSED_RESULT;
    virtual Packet *simple_action(Packet *p);

    virtual bool run_task(Task *task);	// return true iff did useful work
    virtual void run_timer(Timer *timer);
#if CLICK_USERLEVEL
    enum { SELECT_READ = 1, SELECT_WRITE = 2 };
    virtual void selected(int fd, int mask);
    virtual void selected(int fd);
#endif

    inline void checked_output_push(int port, Packet *p) const;
    inline Packet* checked_input_pull(int port) const;

    // ELEMENT CHARACTERISTICS
    virtual const char *class_name() const = 0;

    virtual const char *port_count() const;
    static const char PORTS_0_0[];
    static const char PORTS_0_1[];
    static const char PORTS_1_0[];
    static const char PORTS_1_1[];
    static const char PORTS_1_1X2[];

    virtual const char *processing() const;
    static const char AGNOSTIC[];
    static const char PUSH[];
    static const char PULL[];
    static const char PUSH_TO_PULL[];
    static const char PULL_TO_PUSH[];
    static const char PROCESSING_A_AH[];

    virtual const char *flow_code() const;
    static const char COMPLETE_FLOW[];

    virtual const char *flags() const;
    int flag_value(int flag) const;

    virtual void *cast(const char *name);
    virtual void *port_cast(bool isoutput, int port, const char *name);

    // CONFIGURATION, INITIALIZATION, AND CLEANUP
    enum ConfigurePhase {
	CONFIGURE_PHASE_FIRST = 0,
	CONFIGURE_PHASE_INFO = 20,
	CONFIGURE_PHASE_PRIVILEGED = 90,
	CONFIGURE_PHASE_DEFAULT = 100,
	CONFIGURE_PHASE_LAST = 2000
    };
    virtual int configure_phase() const;

    virtual int configure(Vector<String> &conf, ErrorHandler *errh);

    virtual void add_handlers();

    virtual int initialize(ErrorHandler *errh);

    virtual void take_state(Element *old_element, ErrorHandler *errh);
    virtual Element *hotswap_element() const;

    enum CleanupStage {
	CLEANUP_NO_ROUTER,
	CLEANUP_BEFORE_CONFIGURE = CLEANUP_NO_ROUTER,
	CLEANUP_CONFIGURE_FAILED,
	CLEANUP_CONFIGURED,
	CLEANUP_INITIALIZE_FAILED,
	CLEANUP_INITIALIZED,
	CLEANUP_ROUTER_INITIALIZED,
	CLEANUP_MANUAL
    };
    virtual void cleanup(CleanupStage stage);

    static inline void static_initialize();
    static inline void static_cleanup();

    // ELEMENT ROUTER CONNECTIONS
    String name() const;
    virtual String declaration() const;

    inline Router *router() const;
    inline int eindex() const;
    inline int eindex(Router *r) const;

    /** @brief Return the element's master. */
    inline Master *master() const;

    inline void attach_router(Router *r, int eindex) {
	assert(!_router);
	_router = r;
	_eindex = eindex;
    }

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
    String configuration() const;

    virtual bool can_live_reconfigure() const;
    virtual int live_reconfigure(Vector<String>&, ErrorHandler*);

    RouterThread *home_thread() const;

#if CLICK_USERLEVEL
    // SELECT
    int add_select(int fd, int mask);
    int remove_select(int fd, int mask);
#endif

    // HANDLERS
    void add_read_handler(const String &name, ReadHandlerCallback read_callback, const void *user_data = 0, uint32_t flags = 0);
    void add_read_handler(const String &name, ReadHandlerCallback read_callback, int user_data, uint32_t flags = 0);
    void add_read_handler(const char *name, ReadHandlerCallback read_callback, int user_data = 0, uint32_t flags = 0);
    void add_write_handler(const String &name, WriteHandlerCallback write_callback, const void *user_data = 0, uint32_t flags = 0);
    void add_write_handler(const String &name, WriteHandlerCallback write_callback, int user_data, uint32_t flags = 0);
    void add_write_handler(const char *name, WriteHandlerCallback write_callback, int user_data = 0, uint32_t flags = 0);
    void set_handler(const String &name, int flags, HandlerCallback callback, const void *read_user_data = 0, const void *write_user_data = 0);
    void set_handler(const String &name, int flags, HandlerCallback callback, int read_user_data, int write_user_data = 0);
    void set_handler(const char *name, int flags, HandlerCallback callback, int read_user_data = 0, int write_user_data = 0);
    int set_handler_flags(const String &name, int set_flags, int clear_flags = 0);
    enum { TASKHANDLER_WRITE_SCHEDULED = 1,
	   TASKHANDLER_WRITE_TICKETS = 2,
	   TASKHANDLER_WRITE_HOME_THREAD = 4,
	   TASKHANDLER_WRITE_ALL = 7,
	   TASKHANDLER_DEFAULT = 6 };
    void add_task_handlers(Task *task, NotifierSignal *signal, int flags, const String &prefix = String());
    inline void add_task_handlers(Task *task, NotifierSignal *signal, const String &prefix = String()) {
	add_task_handlers(task, signal, TASKHANDLER_DEFAULT, prefix);
    }
    inline void add_task_handlers(Task *task, const String &prefix = String()) {
	add_task_handlers(task, 0, TASKHANDLER_DEFAULT, prefix);
    }

    void add_data_handlers(const char *name, int flags, uint8_t *data);
    void add_data_handlers(const char *name, int flags, bool *data);
    void add_data_handlers(const char *name, int flags, uint16_t *data);
    void add_data_handlers(const char *name, int flags, int *data);
    void add_data_handlers(const char *name, int flags, unsigned *data);
    void add_data_handlers(const char *name, int flags, atomic_uint32_t *data);
    void add_data_handlers(const char *name, int flags, long *data);
    void add_data_handlers(const char *name, int flags, unsigned long *data);
#if HAVE_LONG_LONG
    void add_data_handlers(const char *name, int flags, long long *data);
    void add_data_handlers(const char *name, int flags, unsigned long long *data);
#endif
    void add_net_order_data_handlers(const char *name, int flags, uint16_t *data);
    void add_net_order_data_handlers(const char *name, int flags, uint32_t *data);
#if HAVE_FLOAT_TYPES
    void add_data_handlers(const char *name, int flags, double *data);
#endif
    void add_data_handlers(const char *name, int flags, String *data);
    void add_data_handlers(const char *name, int flags, IPAddress *data);
    void add_data_handlers(const char *name, int flags, EtherAddress *data);
    void add_data_handlers(const char *name, int flags, Timestamp *data, bool is_interval = false);

    static String read_positional_handler(Element*, void*);
    static String read_keyword_handler(Element*, void*);
    static int reconfigure_positional_handler(const String&, Element*, void*, ErrorHandler*);
    static int reconfigure_keyword_handler(const String&, Element*, void*, ErrorHandler*);

    virtual int llrpc(unsigned command, void* arg);
    int local_llrpc(unsigned command, void* arg);

    class Port { public:

	inline bool active() const;
	inline Element* element() const;
	inline int port() const;

	inline void push(Packet* p) const;
	inline Packet* pull() const;

#if CLICK_STATS >= 1
	unsigned npackets() const	{ return _packets; }
#endif

	inline void assign(bool isoutput, Element *e, int port);

      private:

	Element* _e;
	int _port;
#if HAVE_BOUND_PORT_TRANSFER
	union {
	    void (*push)(Element *e, int port, Packet *p);
	    Packet *(*pull)(Element *e, int port);
	} _bound;
#endif

#if CLICK_STATS >= 1
	mutable unsigned _packets;	// How many packets have we moved?
#endif
#if CLICK_STATS >= 2
	Element* _owner;		// Whose input or output are we?
#endif

	inline Port();
	inline void assign(bool isoutput, Element *owner, Element *e, int port);

	friend class Element;

    };

    // DEPRECATED
    /** @cond never */
    String id() const CLICK_DEPRECATED;
    String landmark() const CLICK_DEPRECATED;
    /** @endcond never */

  private:

    enum { INLINE_PORTS = 4 };

    Port* _ports[2];
    Port _inline_ports[INLINE_PORTS];

    int _nports[2];

    Router* _router;
    int _eindex;

#if CLICK_STATS >= 2
    // STATISTICS
    unsigned _xfer_calls;	// Push and pull calls into this element.
    click_cycles_t _xfer_own_cycles;	// Cycles spent in self from push and pull.
    click_cycles_t _child_cycles;	// Cycles spent in children.

    unsigned _task_calls;	// Calls to tasks owned by this element.
    click_cycles_t _task_own_cycles;	// Cycles spent in self from tasks.

    unsigned _timer_calls;	// Calls to timers owned by this element.
    click_cycles_t _timer_own_cycles;	// Cycles spent in self from timers.

    inline void reset_cycles() {
	_xfer_calls = _task_calls = _timer_calls = 0;
	_xfer_own_cycles = _task_own_cycles = _timer_own_cycles = _child_cycles = 0;
    }
    static String read_cycles_handler(Element *, void *);
    static int write_cycles_handler(const String &, Element *, void *, ErrorHandler *);
#endif

    Element(const Element &);
    Element &operator=(const Element &);

    // METHODS USED BY ROUTER
    int set_nports(int, int);
    int notify_nports(int, int, ErrorHandler *);
    enum Processing { VAGNOSTIC, VPUSH, VPULL };
    static int next_processing_code(const char*& p, ErrorHandler* errh);
    void processing_vector(int* input_codes, int* output_codes, ErrorHandler*) const;

    void initialize_ports(const int* input_codes, const int* output_codes);
    int connect_port(bool isoutput, int port, Element*, int);

    static String read_handlers_handler(Element *e, void *user_data);
    void add_default_handlers(bool writable_config);
    inline void add_data_handlers(const char *name, int flags, HandlerCallback callback, void *data);

    friend class Router;
#if CLICK_STATS >= 2
    friend class Task;
    friend class Master;
    friend class TimerSet;
# if CLICK_USERLEVEL
    friend class SelectSet;
# endif
#endif

};


/** @brief Initialize static data for this element class.
 *
 * Place initialization code for an element class's shared global state in the
 * static_initialize() static member function.  (For example, the IPFilter
 * element class uses static_initialize() to set up various parsing tables.)
 * Click drivers will call this function when the element code is loaded,
 * before any elements of the class are created.
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
 * @note In most cases you should also define a static_cleanup() function to
 * clean up state initialized by static_initialize().
 *
 * @sa Element::static_cleanup
 */
inline void
Element::static_initialize()
{
}

/** @brief Clean up static data for this element class.
 *
 * Place cleanup code for an element class's shared global state in the
 * static_cleanup() static member function.  Click drivers will call this
 * function before unloading the element code.
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
# define PORT_ASSIGN(o) _packets = 0; _owner = (o)
#elif CLICK_STATS >= 1
# define PORT_ASSIGN(o) _packets = 0; (void) (o)
#else
# define PORT_ASSIGN(o) (void) (o)
#endif

inline
Element::Port::Port()
    : _e(0), _port(-2)
{
    PORT_ASSIGN(0);
}

inline void
Element::Port::assign(bool isoutput, Element *e, int port)
{
    _e = e;
    _port = port;
    (void) isoutput;
#if HAVE_BOUND_PORT_TRANSFER
    if (e) {
	if (isoutput) {
	    void (Element::*pusher)(int, Packet *) = &Element::push;
	    _bound.push = (void (*)(Element *, int, Packet *)) (e->*pusher);
	} else {
	    Packet *(Element::*puller)(int) = &Element::pull;
	    _bound.pull = (Packet *(*)(Element *, int)) (e->*puller);
	}
    }
#endif
}

inline void
Element::Port::assign(bool isoutput, Element *owner, Element *e, int port)
{
    PORT_ASSIGN(owner);
    assign(isoutput, e, port);
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
    assert(_e && p);
#if CLICK_STATS >= 1
    ++_packets;
#endif
#if CLICK_STATS >= 2
    ++_e->input(_port)._packets;
    click_cycles_t start_cycles = click_get_cycles(),
	start_child_cycles = _e->_child_cycles;
# if HAVE_BOUND_PORT_TRANSFER
    _bound.push(_e, _port, p);
# else
    _e->push(_port, p);
# endif
    click_cycles_t all_delta = click_get_cycles() - start_cycles,
	own_delta = all_delta - (_e->_child_cycles - start_child_cycles);
    _e->_xfer_calls += 1;
    _e->_xfer_own_cycles += own_delta;
    _owner->_child_cycles += all_delta;
#else
# if HAVE_BOUND_PORT_TRANSFER
    _bound.push(_e, _port, p);
# else
    _e->push(_port, p);
# endif
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
    click_cycles_t start_cycles = click_get_cycles(),
	old_child_cycles = _e->_child_cycles;
# if HAVE_BOUND_PORT_TRANSFER
    Packet *p = _bound.pull(_e, _port);
# else
    Packet *p = _e->pull(_port);
# endif
    if (p)
	_e->output(_port)._packets += 1;
    click_cycles_t all_delta = click_get_cycles() - start_cycles,
	own_delta = all_delta - (_e->_child_cycles - old_child_cycles);
    _e->_xfer_calls += 1;
    _e->_xfer_own_cycles += own_delta;
    _owner->_child_cycles += all_delta;
#else
# if HAVE_BOUND_PORT_TRANSFER
    Packet *p = _bound.pull(_e, _port);
# else
    Packet *p = _e->pull(_port);
# endif
#endif
#if CLICK_STATS >= 1
    if (p)
	++_packets;
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

/** @brief Pull a packet from input @a port, or return 0 if @a port is out of
 * range.
 *
 * @param port input port number
 *
 * If @a port is in range (>= 0 and < ninputs()), then return the result
 * of input(@a port).pull().  Otherwise, return null.
 *
 * @note It is invalid to call checked_input_pull() on a push input @a port.
 */
inline Packet*
Element::checked_input_pull(int port) const
{
    if ((unsigned) port < (unsigned) ninputs())
	return _ports[0][port].pull();
    else
	return 0;
}

#undef PORT_ASSIGN
CLICK_ENDDECLS
#endif
