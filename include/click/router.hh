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
class RouterVisitor;
class RouterThread;
class HashMap_ArenaFactory;
class NotifierSignal;
class ThreadSched;
class Handler;
class NameInfo;

class Router { public:

    /** @name Public Member Functions */
    //@{
    // MASTER
    inline Master* master() const;

    // STATUS
    inline bool initialized() const;
    inline bool handlers_ready() const;
    inline bool running() const;

    // RUNCOUNT AND RUNCLASS
    enum { STOP_RUNCOUNT = -2147483647 - 1 };
    inline int32_t runcount() const;
    void adjust_runcount(int32_t delta);
    void set_runcount(int32_t rc);
    inline void please_stop_driver();

    // ELEMENTS
    inline const Vector<Element*>& elements() const;
    inline int nelements() const;
    inline Element* element(int i) const;
    inline Element* root_element() const;
    static Element* element(const Router *router, int i);

    const String& ename(int i) const;
    String ename_context(int i) const;
    String elandmark(int i) const;
    const String& econfiguration(int i) const;
    void set_econfiguration(int i, const String& conf);

    Element* find(const String& name, ErrorHandler* errh = 0) const;
    Element* find(const String& name, String context, ErrorHandler* errh = 0) const;
    Element* find(const String& name, const Element* context, ErrorHandler* errh = 0) const;

    int visit(Element *e, bool isoutput, int port, RouterVisitor *visitor) const;
    int visit_downstream(Element *e, int port, RouterVisitor *visitor) const;
    int visit_upstream(Element *e, int port, RouterVisitor *visitor) const;

    int downstream_elements(Element *e, int port, ElementFilter *filter, Vector<Element*> &result) CLICK_DEPRECATED;
    int upstream_elements(Element *e, int port, ElementFilter *filter, Vector<Element*> &result) CLICK_DEPRECATED;

    inline const char *flow_code_override(int eindex) const;
    void set_flow_code_override(int eindex, const String &flow_code);

    // HANDLERS
    // 'const Handler *' results last until that element/handlername modified
    static const Handler *handler(const Element *e, const String &hname);
    static void add_read_handler(const Element *e, const String &hname, ReadHandlerCallback callback, void *user_data, uint32_t flags = 0);
    static void add_write_handler(const Element *e, const String &hname, WriteHandlerCallback callback, void *user_data, uint32_t flags = 0);
    static void set_handler(const Element *e, const String &hname, uint32_t flags, HandlerCallback callback, void *read_user_data = 0, void *write_user_data = 0);
    static int set_handler_flags(const Element *e, const String &hname, uint32_t set_flags, uint32_t clear_flags = 0);

    enum { FIRST_GLOBAL_HANDLER = 0x40000000 };
    static int hindex(const Element *e, const String &hname);
    static const Handler *handler(const Router *router, int hindex);
    static void element_hindexes(const Element *e, Vector<int> &result);

    // ATTACHMENTS AND REQUIREMENTS
    void* attachment(const String& aname) const;
    void*& force_attachment(const String& aname);
    void* set_attachment(const String& aname, void* value);

    ErrorHandler* chatter_channel(const String& channel_name) const;
    HashMap_ArenaFactory* arena_factory() const;

    inline ThreadSched* thread_sched() const;
    inline void set_thread_sched(ThreadSched* scheduler);
    inline int home_thread_id(const Element *e) const;

    /** @cond never */
    // Needs to be public for NameInfo, but not useful outside
    inline NameInfo* name_info() const;
    NameInfo* force_name_info();
    /** @endcond never */

    // UNPARSING
    String configuration_string() const;
    void unparse(StringAccum& sa, const String& indent = String()) const;
    void unparse_requirements(StringAccum& sa, const String& indent = String()) const;
    void unparse_declarations(StringAccum& sa, const String& indent = String()) const;
    void unparse_connections(StringAccum& sa, const String& indent = String()) const;

    String element_ports_string(const Element *e) const;
    //@}

    // INITIALIZATION
    /** @name Internal Functions */
    //@{
    Router(const String& configuration, Master* master);
    ~Router();

    static void static_initialize();
    static void static_cleanup();

    inline void use();
    void unuse();

    void add_requirement(const String &type, const String &value);
    int add_element(Element *e, const String &name, const String &conf, const String &filename, unsigned lineno);
    int add_connection(int from_idx, int from_port, int to_idx, int to_port);
#if CLICK_LINUXMODULE
    int add_module_ref(struct module* module);
#endif

    inline Router* hotswap_router() const;
    void set_hotswap_router(Router* router);

    int initialize(ErrorHandler* errh);
    void activate(bool foreground, ErrorHandler* errh);
    inline void activate(ErrorHandler* errh);
    inline void set_foreground(bool foreground);

    int new_notifier_signal(const char *name, NotifierSignal &signal);
    String notifier_signal_name(const atomic_uint32_t *signal) const;
    //@}

    /** @cond never */
    // Needs to be public for Lexer, etc., but not useful outside
    struct Port {
        int idx;
        int port;

        Port() {
        }
        Port(int i, int p)
            : idx(i), port(p) {
        }

        bool operator==(const Port &x) const {
            return idx == x.idx && port == x.port;
        }
        bool operator!=(const Port &x) const {
            return idx != x.idx || port != x.port;
        }
        bool operator<(const Port &x) const {
            return idx < x.idx || (idx == x.idx && port < x.port);
        }
        bool operator<=(const Port &x) const {
            return idx < x.idx || (idx == x.idx && port <= x.port);
        }
    };

    struct Connection {
        Port p[2];

        Connection() {
        }
        Connection(int from_idx, int from_port, int to_idx, int to_port) {
            p[0] = Port(to_idx, to_port);
            p[1] = Port(from_idx, from_port);
        }

        const Port &operator[](int i) const {
            assert(i >= 0 && i < 2);
            return p[i];
        }
        Port &operator[](int i) {
            assert(i >= 0 && i < 2);
            return p[i];
        }

        bool operator==(const Connection &x) const {
            return p[0] == x.p[0] && p[1] == x.p[1];
        }
        bool operator<(const Connection &x) const {
            return p[0] < x.p[0] || (p[0] == x.p[0] && p[1] < x.p[1]);
        }
    };
    /** @endcond never */

#if CLICK_NS
    simclick_node_t *simnode() const;
    int sim_get_ifid(const char* ifname);
    int sim_listen(int ifid, int element);
    int sim_if_ready(int ifid);
    int sim_write(int ifid, int ptype, const unsigned char *, int len,
                  simclick_simpacketinfo *pinfo);
    int sim_incoming_packet(int ifid, int ptype, const unsigned char *,
                            int len, simclick_simpacketinfo* pinfo);
    void sim_trace(const char* event);
    int sim_get_node_id();
    int sim_get_next_pkt_id();
    int sim_if_promisc(int ifid);

  protected:
    Vector<Vector<int> *> _listenvecs;
    Vector<int>* sim_listenvec(int ifid);
#endif

  private:

    class RouterContextErrh;

    enum {
        ROUTER_NEW, ROUTER_PRECONFIGURE, ROUTER_PREINITIALIZE,
        ROUTER_LIVE, ROUTER_DEAD                // order is important
    };
    enum {
        RUNNING_DEAD = -2, RUNNING_INACTIVE = -1, RUNNING_PREPARING = 0,
        RUNNING_BACKGROUND = 1, RUNNING_ACTIVE = 2
    };

    Master* _master;

    atomic_uint32_t _runcount;

    volatile int _state;
    bool _have_connections : 1;
    mutable bool _conn_sorted : 1;
    bool _have_configuration : 1;
    volatile int _running;

    atomic_uint32_t _refcount;

    Vector<Element *> _elements;
    Vector<String> _element_names;
    Vector<String> _element_configurations;
    Vector<uint32_t> _element_landmarkids;
    mutable Vector<int> _element_home_thread_ids;

    struct element_landmark_t {
        uint32_t first_landmarkid;
        String filename;
    };
    Vector<element_landmark_t> _element_landmarks;
    uint32_t _last_landmarkid;

    mutable Vector<int> _element_name_sorter;
    Vector<int> _element_gport_offset[2];
    Vector<int> _element_configure_order;

    mutable Vector<Connection> _conn;
    mutable Vector<int> _conn_output_sorter;

    Vector<String> _requirements;

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

    struct notifier_signals_t {
        enum { capacity = 4096 };
        String name;
        int nsig;
        atomic_uint32_t sig[capacity / 32];
        notifier_signals_t *next;
        notifier_signals_t(const String &n, notifier_signals_t *nx)
            : name(n), nsig(0), next(nx) {
            memset(&sig[0], 0, sizeof(sig));
        }
    };
    notifier_signals_t *_notifier_signals;
    HashMap_ArenaFactory* _arena_factory;
    Router* _hotswap_router;
    ThreadSched* _thread_sched;
    mutable NameInfo* _name_info;
    Vector<int> _flow_code_override_eindex;
    Vector<String> _flow_code_override;

    Router* _next_router;

#if CLICK_LINUXMODULE
    Vector<struct module*> _modules;
#endif

    Router(const Router&);
    Router& operator=(const Router&);

    Connection *remove_connection(Connection *cp);
    void hookup_error(const Port &p, bool isoutput, const char *message,
                      ErrorHandler *errh, bool active = false);
    int check_hookup_elements(ErrorHandler*);
    int check_hookup_range(ErrorHandler*);
    int check_hookup_completeness(ErrorHandler*);

    const char *hard_flow_code_override(int e) const;
    int processing_error(const Connection &conn, bool, int, ErrorHandler*);
    int check_push_and_pull(ErrorHandler*);

    void set_connections();
    void sort_connections() const;
    int connindex_lower_bound(bool isoutput, const Port &port) const;

    void make_gports();
    inline int ngports(bool isout) const {
        return _element_gport_offset[isout].back();
    }
    inline int gport(bool isoutput, const Port &port) const;

    int hard_home_thread_id(const Element *e) const;

    int element_lerror(ErrorHandler*, Element*, const char*, ...) const;

    // private handler methods
    void initialize_handlers(bool, bool);
    inline Handler* xhandler(int) const;
    int find_ehandler(int, const String&, bool allow_star) const;
    static inline Handler fetch_handler(const Element*, const String&);
    void store_local_handler(int eindex, Handler &h);
    static void store_global_handler(Handler &h);
    static inline void store_handler(const Element *element, Handler &h);

    // global handlers
    static String router_read_handler(Element *e, void *user_data);
    static int router_write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh);

    /** @cond never */
    friend class Master;
    friend class Task;
    friend int Element::set_nports(int, int);
    /** @endcond never */

};


/** @brief  Increment the router's reference count.
 *
 *  Routers are reference counted objects.  A Router is created with one
 *  reference, which is held by its Master object.  Normally the Router and
 *  all its elements will be deleted when the Master drops this reference, but
 *  you can preserve the Router for longer by adding a reference yourself. */
inline void
Router::use()
{
    _refcount++;
}

/** @brief  Return true iff the router is currently running.
 *
 *  A running router has been successfully initialized (so running() implies
 *  initialized()), and has not stopped yet. */
inline bool
Router::running() const
{
    return _running > 0;
}

/** @brief  Return true iff the router has been successfully initialized. */
inline bool
Router::initialized() const
{
    return _state == ROUTER_LIVE;
}

/** @brief  Return true iff the router's handlers have been initialized.
 *
 *  handlers_ready() returns false until each element's
 *  Element::add_handlers() method has been called.  This happens after
 *  Element::configure(), but before Element::initialize(). */
inline bool
Router::handlers_ready() const
{
    return _state > ROUTER_PRECONFIGURE;
}

/** @brief  Returns a vector containing all the router's elements.
 *  @invariant  elements()[i] == element(i) for all i in range. */
inline const Vector<Element*>&
Router::elements() const
{
    return _elements;
}

/** @brief  Returns the number of elements in the router. */
inline int
Router::nelements() const
{
    return _elements.size();
}

/** @brief  Returns the element with index @a i.
 *  @param  i  element index, or -1 for root_element()
 *  @invariant  If element(i) isn't null, then element(i)->@link Element::eindex eindex@endlink() == i.
 *
 *  This function returns the element with index @a i.  If @a i ==
 *  -1, returns root_element().  If @a i is otherwise out of range,
 *  returns null. */
inline Element*
Router::element(int i) const
{
    return element(this, i);
}

/** @brief  Returns this router's root element.
 *
 *  Every router has a root Element.  This element has Element::eindex() -1 and
 *  name "".  It is not configured or initialized, and doesn't appear in the
 *  configuration; it exists only for convenience, when other Click code needs
 *  to refer to some arbitrary element at the top level of the compound
 *  element hierarchy. */
inline Element*
Router::root_element() const
{
    return _root_element;
}

inline ThreadSched*
Router::thread_sched() const
{
    return _thread_sched;
}

inline void
Router::set_thread_sched(ThreadSched* ts)
{
    _thread_sched = ts;
}

inline int
Router::home_thread_id(const Element *e) const
{
    if (initialized())
        return _element_home_thread_ids[e->eindex() + 1];
    else
        return hard_home_thread_id(e);
}

/** @cond never */
/** @brief  Return the NameInfo object for this router, if it exists.
 *
 * Users never need to call this. */
inline NameInfo*
Router::name_info() const
{
    return _name_info;
}
/** @endcond never */

/** @brief  Return the Master object for this router. */
inline Master*
Router::master() const
{
    return _master;
}

/** @brief  Return the router's runcount.
 *
 *  The runcount is an integer that determines whether the router is running.
 *  A running router has positive runcount.  Decrementing the router's runcount
 *  to zero or below will cause the router to stop, although elements like
 *  DriverManager can intercept the stop request and continue processing.
 *
 *  Elements request that the router stop its processing by calling
 *  adjust_runcount() or please_stop_driver(). */
inline int32_t
Router::runcount() const
{
    return _runcount.value();
}

/** @brief  Request a driver stop by adjusting the runcount by -1.
 *  @note   Equivalent to adjust_runcount(-1). */
inline void
Router::please_stop_driver()
{
    adjust_runcount(-1);
}

/** @brief Returns the overriding flow code for element @a e, if any.
 *  @param eindex element index
 *  @return The flow code, or null if none has been set. */
inline const char *
Router::flow_code_override(int eindex) const
{
    if (_flow_code_override.size())
        return hard_flow_code_override(eindex);
    else
        return 0;
}

inline void
Router::activate(ErrorHandler* errh)
{
    activate(true, errh);
}

inline void
Router::set_foreground(bool foreground)
{
    assert(_running >= RUNNING_BACKGROUND);
    _running = foreground ? RUNNING_ACTIVE : RUNNING_BACKGROUND;
}

/** @brief  Finds an element named @a name.
 *  @param  name     element name
 *  @param  errh     optional error handler
 *
 *  Returns the unique element named @a name, if any.  If no element named @a
 *  name is found, reports an error to @a errh and returns null.  The error is
 *  "<tt>no element named 'name'</tt>".  If @a errh is null, no error is
 *  reported.
 *
 *  This function is equivalent to find(const String&, String, ErrorHandler*)
 *  with a context argument of the empty string. */
inline Element *
Router::find(const String& name, ErrorHandler *errh) const
{
    return find(name, "", errh);
}

/** @brief Traverse the router configuration downstream of @a e[@a port].
 * @param e element to start search
 * @param port output port (or -1 to search all output ports)
 * @param visitor RouterVisitor traversal object
 * @return 0 on success, -1 in early router configuration stages
 *
 * Calls @a visitor ->@link RouterVisitor::visit visit() @endlink on each
 * reachable input port starting from the output port @a e[@a port].  Follows
 * connections and traverses inside elements from port to port by
 * Element::flow_code().  The visitor can stop a traversal path by returning
 * false from visit().
 *
 * @sa visit_upstream(), visit()
 */
inline int
Router::visit_downstream(Element *e, int port, RouterVisitor *visitor) const
{
    return visit(e, true, port, visitor);
}

/** @brief Traverse the router configuration upstream of [@a port]@a e.
 * @param e element to start search
 * @param port input port (or -1 to search all input ports)
 * @param visitor RouterVisitor traversal object
 * @return 0 on success, -1 in early router configuration stages
 *
 * Calls @a visitor ->@link RouterVisitor::visit visit() @endlink on each
 * reachable output port starting from the input port [@a port]@a e.  Follows
 * connections and traverses inside elements from port to port by
 * Element::flow_code().  The visitor can stop a traversal path by returning
 * false from visit().
 *
 * @sa visit_downstream(), visit()
 */
inline int
Router::visit_upstream(Element *e, int port, RouterVisitor *visitor) const
{
    return visit(e, false, port, visitor);
}

inline HashMap_ArenaFactory*
Router::arena_factory() const
{
    return _arena_factory;
}

/** @brief Returns the currently-installed router this router will eventually
 * replace.
 *
 * This function is only meaningful during a router's initialization.  If this
 * router was installed with the hotswap option, then hotswap_router() will
 * return the currently-installed router that this router will eventually
 * replace (assuming error-free initialization).  Otherwise, hotswap_router()
 * will return 0.
 */
inline Router*
Router::hotswap_router() const
{
    return _hotswap_router;
}

inline
Handler::Handler(const String &name)
    : _name(name), _read_user_data(0), _write_user_data(0), _flags(0),
      _use_count(0), _next_by_name(-1)
{
    _read_hook.r = 0;
    _write_hook.w = 0;
}

CLICK_ENDDECLS
#endif
