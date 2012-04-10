// -*- c-basic-offset: 4; related-file-name: "../../lib/notifier.cc" -*-
#ifndef CLICK_NOTIFIER_HH
#define CLICK_NOTIFIER_HH
#include <click/task.hh>
#include <click/atomic.hh>
#include <click/algorithm.hh>
#if HAVE_CXX_PRAGMA_INTERFACE
# pragma interface "click/notifier.hh"
#endif
CLICK_DECLS

class NotifierSignal { public:

    typedef uint8_t value_type;

    /** @brief Construct a busy signal.
     *
     * The returned signal is always active. */
    inline NotifierSignal();

    /** @brief Construct an activity signal.
     *
     * Elements should not use this constructor directly.
     * @sa Router::new_notifier_signal */
    explicit inline NotifierSignal(value_type* value);

    /** @brief Copy construct a signal. */
    inline NotifierSignal(const NotifierSignal &x);

    /** @brief Destroy a signal. */
    inline ~NotifierSignal();

    /** @brief Return an idle signal.
     *
     * The returned signal is never active. */
    static inline NotifierSignal idle_signal();

    /** @brief Return a busy signal.
     *
     * The returned signal is always active. */
    static inline NotifierSignal busy_signal();

    /** @brief Return an overderived busy signal.
     *
     * Overderived signals replace derived signals that are too complex to
     * represent.  An overderived signal, like a busy signal, is always
     * active. */
    static inline NotifierSignal overderived_signal();

    /** @brief Return an uninitialized signal.
     *
     * Uninitialized signals may be used occasionally as placeholders for true
     * signals to be added later.  Uninitialized signals are never active. */
    static inline NotifierSignal uninitialized_signal();

    typedef bool (NotifierSignal::*unspecified_bool_type)() const;

    /** @brief Return whether the signal is active.
     * @return true iff the signal is currently active. */
    inline operator unspecified_bool_type() const;

    /** @brief Return whether the signal is active.
     * @return true iff the signal is currently active. */
    inline bool active() const;

    /** @brief Return whether the signal is idle.
     * @return true iff the signal is idle, i.e. it will never be active. */
    inline bool idle() const;

    /** @brief Return whether the signal is busy.
     * @return true iff the signal is busy, i.e. it will always be active.
     *
     * @note An overderived_signal() is busy(), but a busy_signal() is not
     * overderived(). */
    inline bool busy() const;

    /** @brief Return whether the signal is overderived.
     * @return true iff the signal equals overderived_signal().
     *
     * @note An overderived_signal() is busy(), but a busy_signal() is not
     * overderived(). */
    inline bool overderived() const;

    /** @brief Return whether the signal is initialized.
     * @return true iff the signal doesn't equal uninitialized_signal(). */
    inline bool initialized() const;

    /** @brief Set whether the basic signal is active.
     * @param active true iff the basic signal is active
     * @return previous active state
     *
     * Use this function to set whether a basic signal is active.
     *
     * It is illegal to call set_active() on derived, idle, busy, or
     * overderived signals.  Some of these actions may cause an assertion
     * failure. */
    inline bool set_active(bool active);

    /** @brief Assign a signal. */
    NotifierSignal &operator=(const NotifierSignal &x);

    /** @brief Exchange the values of this signal and @a x. */
    void swap(NotifierSignal &x) {
	click_swap(_v, x._v);
    }

    /** @brief Make this signal derived by adding information from @a x.
     * @param x the signal to add
     *
     * Creates a derived signal that combines information from this signal and
     * @a x.  Equivalent to "*this = (*this + @a x)".
     *
     * @sa operator+(NotifierSignal, const NotifierSignal&) */
    NotifierSignal &operator+=(const NotifierSignal& x);

    /** @brief Return a human-readable representation of the signal.
     * @param router the relevant router or null
     *
     * Useful for signal debugging. */
    String unparse(Router *router) const;

    static void static_initialize();

    /** @relates NotifierSignal
     * @brief Compare two NotifierSignals for equality.
     *
     * Returns true iff the two NotifierSignals are the same -- i.e., they
     * combine information about exactly the same sets of basic signals.
     *
     * All idle() signals compare equal.  busy_signal() and
     * overderived_signal() do not compare equal, however. */
    friend bool operator==(const NotifierSignal &a, const NotifierSignal &b);

    /** @relates NotifierSignal
     * @brief Compare two NotifierSignals for inequality.
     *
     * Returns true iff !(@a a == @a b). */
    friend bool operator!=(const NotifierSignal &a, const NotifierSignal &b);

    /** @relates NotifierSignal
     * @brief Return a derived signal.
     *
     * Returns a derived signal that combines information from its arguments.
     * The result will be active whenever @a a and/or @a b is active.  If the
     * combination of @a a and @a b is too complex to represent, returns an
     * overderived signal; this trivially follows the invariant since it is
     * always active.
     *
     * Signal derivation is commutative and associative.  The following
     * special combinations are worth remembering:
     *
     *  - An uninitialized signal plus any other signal is uninitialized.
     *  - An idle signal plus any signal @a a equals @a a.
     *  - A busy signal plus any other initialized signal is busy.
     *  - overderived_signal() plus busy_signal() equals busy_signal().
     *
     * @sa NotifierSignal::operator+= */
    friend NotifierSignal operator+(NotifierSignal a, const NotifierSignal &b);

  private:

    struct vmlist {
	value_type marker;
	value_type *p[0];
    };
    union vmvalue {
	value_type *v;
	value_type **vp;
    };

    vmvalue _v;

    enum {
	true_offset = 0, false_offset = 1, overderived_offset = 2,
	uninitialized_offset = 3
    };
    static const value_type static_values[4];

    inline bool is_static() const {
	return static_cast<uintptr_t>(_v.v - static_values) < sizeof(static_values);
    }
    void hard_assign_vm(const NotifierSignal &x);
    void hard_derive_one(value_type *v);
    static bool hard_equals(value_type **a, value_type **b);

};

class Notifier { public:

    enum SearchOp { SEARCH_STOP = 0, SEARCH_CONTINUE, SEARCH_CONTINUE_WAKE };
    typedef void (*callback_type)(void *, Notifier *);

    inline Notifier(SearchOp op = SEARCH_STOP);
    inline Notifier(const NotifierSignal &signal, SearchOp op = SEARCH_STOP);
    virtual ~Notifier();

    /** @brief Return whether the Notifier is initialized. */
    inline bool initialized() const {
	return _signal.initialized();
    }

    int initialize(const char *name, Router *router);

    inline const NotifierSignal &signal() const;
    inline SearchOp search_op() const;

    inline bool active() const;

    inline bool set_active(bool active);
    inline void wake();
    inline void sleep();

    virtual int add_activate_callback(callback_type f, void *user_data);
    virtual void remove_activate_callback(callback_type f, void *user_data);
    inline int add_listener(Task *task);
    inline void remove_listener(Task *task);
    inline int add_dependent_signal(NotifierSignal *signal);
    inline void remove_dependent_signal(NotifierSignal *signal);

    static const char EMPTY_NOTIFIER[];
    static const char FULL_NOTIFIER[];

    static inline NotifierSignal upstream_empty_signal(Element *e, int port);
    static inline NotifierSignal upstream_empty_signal(Element *e, int port, Task *task);
    static inline NotifierSignal upstream_empty_signal(Element *e, int port, Notifier *dependent_notifier);
    static NotifierSignal upstream_empty_signal(Element *e, int port, callback_type f, void *user_data);

    static inline NotifierSignal downstream_full_signal(Element *e, int port);
    static inline NotifierSignal downstream_full_signal(Element *e, int port, Task *task);
    static inline NotifierSignal downstream_full_signal(Element *e, int port, Notifier *dependent_notifier);
    static NotifierSignal downstream_full_signal(Element *e, int port, callback_type f, void *user_data);

    static inline NotifierSignal upstream_empty_signal(Element *e, int port, int) CLICK_DEPRECATED;
    static inline NotifierSignal upstream_empty_signal(Element *e, int port, int, Notifier *) CLICK_DEPRECATED;
    static inline NotifierSignal downstream_full_signal(Element *e, int port, int) CLICK_DEPRECATED;
    static inline NotifierSignal downstream_full_signal(Element *e, int port, int, Notifier *) CLICK_DEPRECATED;

  private:

    NotifierSignal _signal;
    SearchOp _search_op;

    static void dependent_signal_callback(void *, Notifier *);

};

class ActiveNotifier : public Notifier { public:

    ActiveNotifier(SearchOp op = SEARCH_STOP);
    ~ActiveNotifier();

    int add_activate_callback(callback_type f, void *v);
    void remove_activate_callback(callback_type f, void *v);
    void listeners(Vector<Task*> &v) const CLICK_DEPRECATED;

    inline void set_active(bool active, bool schedule = true);
    inline void wake();
    inline void sleep();

#if CLICK_DEBUG_SCHEDULING
    String unparse(Router *router) const;
#endif

  private:

    typedef union {
	Task *t;
	callback_type f;
	void *v;
	uintptr_t p;
    } task_or_signal_t;

    Task* _listener1;
    task_or_signal_t* _listeners;

    int listener_add(callback_type f, void *v);
    int listener_remove(callback_type f, void *v);

    ActiveNotifier(const ActiveNotifier&); // does not exist
    ActiveNotifier& operator=(const ActiveNotifier&); // does not exist

};


inline
NotifierSignal::NotifierSignal()
{
    _v.v = const_cast<value_type*>(&static_values[true_offset]);
}

inline
NotifierSignal::NotifierSignal(value_type* value)
{
    assert(*value < 2);
    _v.v = value;
}

inline
NotifierSignal::NotifierSignal(const NotifierSignal &x)
{
    if (likely(*x._v.v != 2))
	_v = x._v;
    else
	hard_assign_vm(x);
}

inline
NotifierSignal::~NotifierSignal()
{
    if (unlikely(*_v.v == 2))
	delete[] _v.vp;
}

inline NotifierSignal
NotifierSignal::idle_signal()
{
    return NotifierSignal(const_cast<value_type*>(&static_values[false_offset]));
}

inline NotifierSignal
NotifierSignal::busy_signal()
{
    return NotifierSignal(const_cast<value_type*>(&static_values[true_offset]));
}

inline NotifierSignal
NotifierSignal::overderived_signal()
{
    return NotifierSignal(const_cast<value_type*>(&static_values[overderived_offset]));
}

inline NotifierSignal
NotifierSignal::uninitialized_signal()
{
    return NotifierSignal(const_cast<value_type*>(&static_values[uninitialized_offset]));
}

inline bool
NotifierSignal::active() const
{
    // 2012.May.16 This fence is necessary; consider, for example,
    // InfiniteSource's checking of nonfull notifiers.
    click_fence();
    value_type x = *_v.v;
    if (likely(x != 2))
	return x != 0;
    else {
	for (value_type **vm = _v.vp + 1; *vm; ++vm)
	    if (*vm != 0)
		return true;
	return false;
    }
}

inline
NotifierSignal::operator unspecified_bool_type() const
{
    return active() ? &NotifierSignal::active : 0;
}

inline bool
NotifierSignal::idle() const
{
    return _v.v == &static_values[false_offset];
}

inline bool
NotifierSignal::busy() const
{
    return _v.v == &static_values[true_offset] || _v.v == &static_values[overderived_offset];
}

inline bool
NotifierSignal::overderived() const
{
    return _v.v == &static_values[overderived_offset];
}

inline bool
NotifierSignal::initialized() const
{
    return _v.v != &static_values[uninitialized_offset];
}

inline bool
NotifierSignal::set_active(bool active)
{
    assert(!is_static() && *_v.v < 2);
#if !CLICK_USERLEVEL || HAVE_MULTITHREAD
    bool old = active ? __sync_fetch_and_or(_v.v, 1) : __sync_fetch_and_and(_v.v, 254);
#else
    bool old = *_v.v;
    *_v.v = active;
#endif
    return old;
}

inline NotifierSignal &
NotifierSignal::operator=(const NotifierSignal &x)
{
    if (likely(this != &x)) {
	if (unlikely(*_v.v == 2))
	    delete[] _v.vp;
	if (likely(*x._v.v != 2))
	    _v = x._v;
	else
	    hard_assign_vm(x);
    }
    return *this;
}

inline bool
operator==(const NotifierSignal& a, const NotifierSignal& b)
{
    if (a._v.v == b._v.v)
	return true;
    else if ((*a._v.v | *b._v.v) < 2)
	return false;
    else
	return NotifierSignal::hard_equals(a._v.vp, b._v.vp);
}

inline bool
operator!=(const NotifierSignal& a, const NotifierSignal& b)
{
    return !(a == b);
}

inline NotifierSignal
operator+(NotifierSignal a, const NotifierSignal& b)
{
    return a += b;
}

/** @brief Constructs a Notifier.
 * @param op controls notifier path search
 *
 * This function constructs a Notifier object.  The Notifier's associated
 * NotifierSignal is initially idle; it becomes associated with a signal after
 * initialize() is called.
 *
 * The @a op argument controls path search.  The rest of this entry
 * describes it further.
 *
 * Elements interested in notification generally search for Notifier objects
 * along all possible packet paths upstream (or downstream) of one of their
 * ports.  When a Notifier is found along a path, further searching along that
 * path is cut off, so only the closest Notifiers are found.  Sometimes,
 * however, it makes more sense to continue searching for more Notifiers.  The
 * correct behavior is Notifier-specific, and is controlled by this method.
 * When the search encounters a Notifier, it consults the Notifier's @a
 * op variable supplied to the constructor.  It should equal one of
 * three SearchOp constants, which correspond to the following behavior:
 *
 * <dl>
 * <dt>SEARCH_STOP</dt>
 * <dd>Stop searching along this path.  This is the default.</dd>
 * <dt>SEARCH_CONTINUE</dt>
 * <dd>Continue searching along this path.</dd>
 * <dt>SEARCH_CONTINUE_WAKE</dt>
 * <dd>Continue searching along this path, but any further Notifiers should
 * only be used for adding and removing listeners; ignore their NotifierSignal
 * objects.  This operation is useful, for example, for schedulers that store
 * packets temporarily.  Such schedulers provide their own NotifierSignal,
 * since the scheduler may still hold a packet even when all upstream sources
 * are empty, but since they aren't packet sources, they don't know when
 * new packets arrive and can't wake up sleeping listeners.  During
 * initialization, such schedulers should call Notifier::upstream_empty_signal,
 * passing their own Notifier as the fourth argument.  This will ensure that
 * their signal is turned on appropriately whenever an upstream queue becomes
 * nonempty.</dd>
 * </dl>
 */
inline
Notifier::Notifier(SearchOp op)
    : _signal(NotifierSignal::uninitialized_signal()), _search_op(op)
{
}

/** @brief Constructs a Notifier associated with a given signal.
 * @param signal the associated NotifierSignal
 * @param op controls notifier path search
 *
 * This function constructs a Notifier object associated with a specific
 * NotifierSignal, such as NotifierSignal::idle_signal().  Calling
 * initialize() on this Notifier will not change the associated
 * NotifierSignal.  The @a op argument is as in
 * Notifier::Notifier(SearchOp), above.
 */
inline
Notifier::Notifier(const NotifierSignal &signal, SearchOp op)
    : _signal(signal), _search_op(op)
{
}

/** @brief Return this Notifier's associated NotifierSignal.
 *
 * Every Notifier object corresponds to one NotifierSignal; this method
 * returns it.  The signal is @link NotifierSignal::idle idle() @endlink
 * before initialize() is called.
 */
inline const NotifierSignal &
Notifier::signal() const
{
    return _signal;
}

/** @brief Return this Notifier's search operation.
 *
 * @sa Notifier() for a detailed explanation of search operations.
 */
inline Notifier::SearchOp
Notifier::search_op() const
{
    return _search_op;
}

/** @brief Returns whether the associated signal is active.
 *
 * Same as signal().active().
 */
inline bool
Notifier::active() const
{
    return _signal.active();
}

/** @brief Set the associated signal's activity.
 * @param active true iff the signal should be active
 * @return previous active state
 */
inline bool
Notifier::set_active(bool active)
{
    return _signal.set_active(active);
}

/** @brief Set the associated signal to active.
 * @sa set_active
 */
inline void
Notifier::wake()
{
    set_active(true);
}

/** @brief Set the associated signal to inactive.
 * @sa set_active
 */
inline void
Notifier::sleep()
{
    set_active(false);
}

/** @brief Register a listener with this Notifier.
 * @param task Task to reschedule when this Notifier becomes active
 *
 * When this Notifier's associated signal is activated, the Notifier should
 * schedule @a task. Not all types of Notifier provide this functionality. The
 * default implementation does nothing.
 *
 * @sa remove_listener, add_activate_callback, add_dependent_signal
 */
inline int
Notifier::add_listener(Task *task)
{
    return add_activate_callback(0, task);
}

/** @brief Unregister a listener with this Notifier.
 * @param task listener Task
 *
 * Undoes the effect of all prior add_listener(@a task) calls. Does nothing if
 * @a task was never added. The default implementation does nothing.
 *
 * @sa add_listener
 */
inline void
Notifier::remove_listener(Task *task)
{
    remove_activate_callback(0, task);
}

/** @brief Register a dependent signal with this Notifier.
 * @param signal dependent signal
 *
 * When this Notifier's associated signal is activated, the Notifier should
 * also activate @a signal. Not all types of Notifier provide this
 * functionality. The default implementation does nothing.
 *
 * @sa add_listener, add_activate_callback, remove_dependent_signal
 */
inline int
Notifier::add_dependent_signal(NotifierSignal *signal)
{
    return add_activate_callback(dependent_signal_callback, signal);
}

/** @brief Unregister a dependent signal with this Notifier.
 * @param signal dependent signal
 *
 * Undoes the effect of all prior add_dependent_signal(@a signal) calls. Does
 * nothing if @a signal was never added. The default implementation does
 * nothing.
 *
 * @sa add_dependent_signal
 */
inline void
Notifier::remove_dependent_signal(NotifierSignal *signal)
{
    remove_activate_callback(dependent_signal_callback, signal);
}

/** @brief Calculate and return the NotifierSignal derived from all empty
 * notifiers upstream of element @a e's input @a port.
 * @param e an element
 * @param port the input port of @a e at which to start the upstream search
 *
 * Searches the configuration upstream of element @a e's input @a port for @e
 * empty @e notifiers.  These notifiers are associated with packet storage,
 * and should be true when packets are available (or likely to be available
 * quite soon), and false when they are not.  All notifiers found are combined
 * into a single derived signal.  Thus, if any of the base notifiers are
 * active, indicating that at least one packet is available upstream, the
 * derived signal will also be active.  Element @a e's code generally uses the
 * resulting signal to decide whether or not to reschedule itself.
 *
 * The returned signal is generally conservative, meaning that the signal
 * is true whenever a packet exists upstream, but the elements that provide
 * notification are responsible for ensuring this.
 *
 * Overloaded versions of this function can also register a task (as in
 * add_listener()), a signal (as in add_dependent_notifier()), or a callback
 * function (as in add_active_callback()) for each located notifier. When
 * packets become available, the task will be scheduled, the signal will be
 * activated, or the callback will be called.
 *
 * <h3>Supporting upstream_empty_signal()</h3>
 *
 * Elements that have an empty notifier must override the Element::cast()
 * method.  When passed the @a name Notifier::EMPTY_NOTIFIER, this method
 * should return a pointer to the corresponding Notifier object.
 *
 * @sa downstream_full_signal
 */
inline NotifierSignal
Notifier::upstream_empty_signal(Element *e, int port)
{
    return upstream_empty_signal(e, port, (callback_type) 0, 0);
}

/** @brief Calculate and return the NotifierSignal derived from all empty
 * notifiers upstream of element @a e's input @a port.
 * @param e an element
 * @param port the input port of @a e at which to start the upstream search
 * @param task task to schedule when packets become available
 * @sa add_listener */
inline NotifierSignal
Notifier::upstream_empty_signal(Element *e, int port, Task *task)
{
    return upstream_empty_signal(e, port, (callback_type) 0, task);
}

/** @brief Calculate and return the NotifierSignal derived from all empty
 * notifiers upstream of element @a e's input @a port.
 * @param e an element
 * @param port the input port of @a e at which to start the upstream search
 * @param notifier notifier to activate when packets become available
 * @sa add_dependent_signal */
inline NotifierSignal
Notifier::upstream_empty_signal(Element *e, int port, Notifier *dependent_notifier)
{
    return upstream_empty_signal(e, port, dependent_signal_callback, &dependent_notifier->_signal);
}

/** @brief Calculate and return the NotifierSignal derived from all full
 * notifiers downstream of element @a e's output @a port.
 * @param e an element
 * @param port the output port of @a e at which to start the downstream search
 *
 * Searches the configuration downstream of element @a e's output @a port for
 * @e full @e notifiers.  These notifiers are associated with packet storage,
 * and should be true when there is space for at least one packet, and false
 * when there is not.  All notifiers found are combined into a single derived
 * signal.  Thus, if any of the base notifiers are active, indicating that at
 * least one path has available space, the derived signal will also be active.
 * Element @a e's code generally uses the resulting signal to decide whether
 * or not to reschedule itself.
 *
 * Overloaded versions of this function can also register a task (as in
 * add_listener()), a signal (as in add_dependent_notifier()), or a callback
 * function (as in add_active_callback()) for each located notifier. When
 * space becomes available, the task will be scheduled, the signal will be
 * activated, or the callback will be called.
 *
 * In current Click, the returned signal is conservative: if it's inactive,
 * then there is no space for packets downstream.
 *
 * <h3>Supporting downstream_full_signal()</h3>
 *
 * Elements that have a full notifier must override the Element::cast()
 * method.  When passed the @a name Notifier::FULL_NOTIFIER, this method
 * should return a pointer to the corresponding Notifier object.
 *
 * @sa upstream_empty_signal
 */
inline NotifierSignal
Notifier::downstream_full_signal(Element *e, int port)
{
    return downstream_full_signal(e, port, (callback_type) 0, 0);
}

/** @brief Calculate and return the NotifierSignal derived from all full
 * notifiers downstream of element @a e's output @a port.
 * @param e an element
 * @param port the output port of @a e at which to start the downstream search
 * @param task task to schedule when packets become available
 * @sa add_listener */
inline NotifierSignal
Notifier::downstream_full_signal(Element *e, int port, Task *task)
{
    return downstream_full_signal(e, port, (callback_type) 0, task);
}

/** @brief Calculate and return the NotifierSignal derived from all full
 * notifiers downstream of element @a e's output @a port.
 * @param e an element
 * @param port the output port of @a e at which to start the downstream search
 * @param notifier notifier to activate when packets become available
 * @sa add_dependent_signal */
inline NotifierSignal
Notifier::downstream_full_signal(Element *e, int port, Notifier *dependent_notifier)
{
    return downstream_full_signal(e, port, dependent_signal_callback, &dependent_notifier->_signal);
}

/** @cond never */
inline NotifierSignal
Notifier::upstream_empty_signal(Element *e, int port, int x)
{
    (void) x;
    assert(x == 0);
    return upstream_empty_signal(e, port);
}

inline NotifierSignal
Notifier::upstream_empty_signal(Element *e, int port, int x, Notifier *notifier)
{
    (void) x;
    assert(x == 0);
    return upstream_empty_signal(e, port, notifier);
}

inline NotifierSignal
Notifier::downstream_full_signal(Element *e, int port, int x)
{
    (void) x;
    assert(x == 0);
    return downstream_full_signal(e, port);
}

inline NotifierSignal
Notifier::downstream_full_signal(Element *e, int port, int x, Notifier *notifier)
{
    (void) x;
    assert(x == 0);
    return downstream_full_signal(e, port, notifier);
}
/** @endcond never */

/** @brief Set the associated signal's activity, possibly scheduling any
 * listener tasks.
 * @param active true iff the signal should be active
 * @param schedule if true, wake up listener tasks
 *
 * If @a active and @a schedule are both true, and the signal was previously
 * inactive, then any listener Tasks are scheduled with Task::reschedule().
 *
 * @sa wake, sleep, add_listener
 */
inline void
ActiveNotifier::set_active(bool active, bool schedule)
{
    bool was_active = Notifier::set_active(active);
    if (active && schedule && !was_active) {
	// 2007.Sep.6: Perhaps there was a race condition here.  Make sure
	// that we set the notifier to active BEFORE rescheduling downstream
	// tasks.  This is because, in a multithreaded environment, a task we
	// reschedule might run BEFORE we set the notifier; after which it
	// would go to sleep forever.
	if (_listener1)
	    _listener1->reschedule();
	else if (task_or_signal_t *tos = _listeners) {
	    for (; tos->p > 1; tos++)
		tos->t->reschedule();
	    if (tos->p == 1)
		for (tos++; tos->p; tos += 2)
		    tos->f(tos[1].v, this);
	}
    }
}

/** @brief Set the associated signal to active and schedule any listener
 * tasks.
 *
 * If the signal was previously inactive, then any listener Tasks are
 * scheduled with Task::reschedule().
 *
 * @sa set_active, add_listener
 */
inline void
ActiveNotifier::wake()
{
    set_active(true, true);
}

/** @brief Set the associated signal to inactive.
 * @sa set_active
 */
inline void
ActiveNotifier::sleep()
{
    set_active(false, true);
}

inline void
click_swap(NotifierSignal &x, NotifierSignal &y)
{
    x.swap(y);
}

CLICK_ENDDECLS
#endif
