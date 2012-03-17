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

    /** @brief Construct a busy signal.
     *
     * The returned signal is always active. */
    inline NotifierSignal();

    /** @brief Construct an activity signal.
     *
     * Elements should not use this constructor directly.
     * @sa Router::new_notifier_signal */
    inline NotifierSignal(atomic_uint32_t* value, uint32_t mask);

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
	click_swap(_mask, x._mask);
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
     *  - An idle() signal plus any other signal @a a equals @a a.  Thus,
     *    idle_signal() is the identity for signal derivation.
     *  - A busy() signal plus any other signal is busy().  Thus,
     *    busy_signal() is the "zero element" for signal derivation.
     *
     * @sa NotifierSignal::operator+= */
    friend NotifierSignal operator+(NotifierSignal a, const NotifierSignal &b);

  private:

    struct vmpair {
	atomic_uint32_t *value;
	uint32_t mask;
    };
    union vmvalue {
	atomic_uint32_t *v1;
	vmpair *vm;
    };

    vmvalue _v;
    uint32_t _mask;

    enum {
	true_mask = 1, false_mask = 2, overderived_mask = 4,
	uninitialized_mask = 8
    };
    static atomic_uint32_t static_value;

    void hard_assign_vm(const NotifierSignal &x);
    void hard_derive_one(atomic_uint32_t *value, uint32_t mask);
    static bool hard_equals(const vmpair *a, const vmpair *b);

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

    static NotifierSignal upstream_empty_signal(Element* e, int port, Task* task, Notifier* dependent_notifier = 0);
    static NotifierSignal downstream_full_signal(Element* e, int port, Task* task, Notifier* dependent_notifier = 0);

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
    void listeners(Vector<Task*> &v) const;

    inline void set_active(bool active, bool schedule = true);
    inline void wake();
    inline void sleep();

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
    : _mask(true_mask)
{
    _v.v1 = &static_value;
}

inline
NotifierSignal::NotifierSignal(atomic_uint32_t* value, uint32_t mask)
    : _mask(mask)
{
    _v.v1 = value;
}

inline
NotifierSignal::NotifierSignal(const NotifierSignal &x)
    : _mask(x._mask)
{
    if (likely(_mask))
	_v.v1 = x._v.v1;
    else
	hard_assign_vm(x);
}

inline
NotifierSignal::~NotifierSignal()
{
    if (unlikely(_mask == 0))
	delete[] _v.vm;
}

inline NotifierSignal
NotifierSignal::idle_signal()
{
    return NotifierSignal(&static_value, false_mask);
}

inline NotifierSignal
NotifierSignal::busy_signal()
{
    return NotifierSignal(&static_value, true_mask);
}

inline NotifierSignal
NotifierSignal::overderived_signal()
{
    return NotifierSignal(&static_value, overderived_mask | true_mask);
}

inline NotifierSignal
NotifierSignal::uninitialized_signal()
{
    return NotifierSignal(&static_value, uninitialized_mask);
}

inline bool
NotifierSignal::active() const
{
    // 2011.Jul.3 Is this fence actually necessary with the new reschedule()
    // plan??  It doesn't seem to be on one simple test, anyway.
    // click_fence();
    if (likely(_mask))
	return (*_v.v1 & _mask) != 0;
    else {
	for (vmpair *vm = _v.vm; vm->mask; ++vm)
	    if ((*vm->value & vm->mask) != 0)
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
    return (_mask == false_mask && _v.v1 == &static_value);
}

inline bool
NotifierSignal::busy() const
{
    return ((_mask & true_mask) && _v.v1 == &static_value);
}

inline bool
NotifierSignal::overderived() const
{
    return ((_mask & overderived_mask) && _v.v1 == &static_value);
}

inline bool
NotifierSignal::initialized() const
{
    return (!(_mask & uninitialized_mask) || _v.v1 != &static_value);
}

inline bool
NotifierSignal::set_active(bool active)
{
    assert(_v.v1 != &static_value && !(_mask & (_mask - 1)));
    uint32_t expected = *_v.v1;
#if !CLICK_USERLEVEL || HAVE_MULTITHREAD
    while (_mask) {
	uint32_t desired = (active ? expected | _mask : expected & ~_mask);
	uint32_t actual = _v.v1->compare_swap(expected, desired);
	if (expected == actual)
	    break;
	expected = actual;
    }
#else
    *_v.v1 = (active ? expected | _mask : expected & ~_mask);
#endif
    return expected & _mask;
}

inline NotifierSignal &
NotifierSignal::operator=(const NotifierSignal &x)
{
    if (likely(this != &x)) {
	if (unlikely(_mask == 0))
	    delete[] _v.vm;
	_mask = x._mask;
	if (likely(_mask))
	    _v.v1 = x._v.v1;
	else
	    hard_assign_vm(x);
    }
    return *this;
}

inline bool
operator==(const NotifierSignal& a, const NotifierSignal& b)
{
    if (a._mask == b._mask) {
	if (likely(a._mask))
	    return a._v.v1 == b._v.v1;
	else
	    return NotifierSignal::hard_equals(a._v.vm, b._v.vm);
    } else
	return false;
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

inline void click_swap(NotifierSignal &x, NotifierSignal &y)
{
    x.swap(y);
}

CLICK_ENDDECLS
#endif
