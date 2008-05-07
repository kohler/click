// -*- c-basic-offset: 4; related-file-name: "../../lib/notifier.cc" -*-
#ifndef CLICK_NOTIFIER_HH
#define CLICK_NOTIFIER_HH
#include <click/task.hh>
#include <click/atomic.hh>
CLICK_DECLS

class NotifierSignal { public:

    inline NotifierSignal();		// always active
    inline NotifierSignal(atomic_uint32_t* value, uint32_t mask);

    static inline NotifierSignal idle_signal();
    static inline NotifierSignal busy_signal();
    static inline NotifierSignal overderived_signal();
    static inline NotifierSignal uninitialized_signal();

    typedef bool (NotifierSignal::*unspecified_bool_type)() const;
    inline operator unspecified_bool_type() const;
    inline bool active() const;

    inline bool idle() const;
    inline bool busy() const;
    inline bool overderived() const;
    inline bool initialized() const;

    inline void set_active(bool active);

    NotifierSignal& operator+=(const NotifierSignal& x);

    String unparse(Router *router) const;
    
    static void static_initialize();
    
  private:

    atomic_uint32_t* _value;
    uint32_t _mask;

    enum {
	TRUE_MASK = 1, FALSE_MASK = 2, OVERDERIVED_MASK = 4,
	UNINITIALIZED_MASK = 8
    };
    static atomic_uint32_t static_value;
    friend bool operator==(const NotifierSignal&, const NotifierSignal&);
    friend bool operator!=(const NotifierSignal&, const NotifierSignal&);
    friend NotifierSignal operator+(NotifierSignal, const NotifierSignal&);

};

class Notifier { public:

    enum SearchOp { SEARCH_STOP = 0, SEARCH_CONTINUE, SEARCH_CONTINUE_WAKE };
    
    inline Notifier(SearchOp op = SEARCH_STOP);
    inline Notifier(const NotifierSignal &signal, SearchOp op = SEARCH_STOP);
    virtual ~Notifier();

    int initialize(const char *name, Router *router);
    
    inline const NotifierSignal &signal() const;
    inline SearchOp search_op() const;

    inline bool active() const;

    inline void set_active(bool active);
    inline void wake();
    inline void sleep();
    
    virtual int add_listener(Task *task);
    virtual void remove_listener(Task *task);
    virtual int add_dependent_signal(NotifierSignal *signal);

    static const char EMPTY_NOTIFIER[];
    static const char FULL_NOTIFIER[];
    
    static NotifierSignal upstream_empty_signal(Element* e, int port, Task* task, Notifier* dependent_notifier = 0);
    static NotifierSignal downstream_full_signal(Element* e, int port, Task* task, Notifier* dependent_notifier = 0);

  private:

    NotifierSignal _signal;
    SearchOp _search_op;
    
};

class ActiveNotifier : public Notifier { public:

    ActiveNotifier(SearchOp op = SEARCH_STOP);
    ~ActiveNotifier();

    int add_listener(Task *task);	// complains on out of memory
    void remove_listener(Task *task);
    int add_dependent_signal(NotifierSignal *signal);
    void listeners(Vector<Task*> &v) const;

    inline void set_active(bool active, bool schedule = true);
    inline void wake();
    inline void sleep();
    
  private:

    typedef union {
	Task *t;
	NotifierSignal *s;
	void *v;
    } task_or_signal_t;
    
    Task* _listener1;
    task_or_signal_t* _listeners;

    int listener_change(void *what, int where, bool rem);
    
    ActiveNotifier(const ActiveNotifier&); // does not exist
    ActiveNotifier& operator=(const ActiveNotifier&); // does not exist

};


/** @brief Construct a busy signal.
 *
 * The returned signal is always active.
 */
inline
NotifierSignal::NotifierSignal()
    : _value(&static_value), _mask(TRUE_MASK)
{
}

/** @brief Construct an activity signal.
 *
 * Elements should not use this constructor directly.
 * @sa Router::new_notifier_signal
 */
inline
NotifierSignal::NotifierSignal(atomic_uint32_t* value, uint32_t mask)
    : _value(value), _mask(mask)
{
}

/** @brief Return an idle signal.
 *
 * The returned signal is never active.
 */
inline NotifierSignal
NotifierSignal::idle_signal()
{
    return NotifierSignal(&static_value, 0);
}

/** @brief Return a busy signal.
 *
 * The returned signal is always active.
 */
inline NotifierSignal
NotifierSignal::busy_signal()
{
    return NotifierSignal(&static_value, TRUE_MASK);
}

/** @brief Return an overderived busy signal.
 *
 * Overderived signals replace derived signals that are too complex to
 * represent.  An overderived signal, like a busy signal, is always active.
 */
inline NotifierSignal
NotifierSignal::overderived_signal()
{
    return NotifierSignal(&static_value, OVERDERIVED_MASK | TRUE_MASK);
}

/** @brief Return an uninitialized signal.
 *
 * Unintialized signals may be used occasionally as placeholders for true
 * signals to be added later.  Uninitialized signals are never active.
 */
inline NotifierSignal
NotifierSignal::uninitialized_signal()
{
    return NotifierSignal(&static_value, UNINITIALIZED_MASK);
}

/** @brief Return whether the signal is active.
 * @return true iff the signal is currently active.
 */
inline bool
NotifierSignal::active() const
{
    return (*_value & _mask) != 0;
}

/** @brief Return whether the signal is active.
 * @return true iff the signal is currently active.
 */
inline
NotifierSignal::operator unspecified_bool_type() const
{
    return active() ? &NotifierSignal::active : 0;
}

/** @brief Return whether the signal is idle.
 * @return true iff the signal is idle, i.e. it will never be active.
 */
inline bool
NotifierSignal::idle() const
{
    return !_mask;
}

/** @brief Return whether the signal is busy.
 * @return true iff the signal is busy, i.e. it will always be active.
 *
 * @note An overderived_signal() is busy(), but a busy_signal() is not
 * overderived().
 */
inline bool
NotifierSignal::busy() const
{
    return (_value == &static_value && (_mask & TRUE_MASK));
}

/** @brief Return whether the signal is overderived.
 * @return true iff the signal equals overderived_signal().
 *
 * @note An overderived_signal() is busy(), but a busy_signal() is not
 * overderived().
 */
inline bool
NotifierSignal::overderived() const
{
    return (_value == &static_value && (_mask & OVERDERIVED_MASK));
}

/** @brief Return whether the signal is initialized.
 * @return true iff the signal doesn't equal uninitialized_signal().
 */
inline bool
NotifierSignal::initialized() const
{
    return (_value != &static_value || !(_mask & UNINITIALIZED_MASK));
}

/** @brief Set whether the signal is active.
 * @param active true iff the signal is active
 *
 * Use this function to set whether a basic signal is active.
 *
 * It is illegal to call set_active() on derived, idle, busy, or overderived
 * signals.  Some of these actions may cause an assertion failure.
 */
inline void
NotifierSignal::set_active(bool active)
{
    assert(_value != &static_value);
    if (active)
	*_value |= _mask;
    else
	*_value &= ~_mask;
}

/** @relates NotifierSignal
 * @brief Compare two NotifierSignals for equality.
 *
 * Returns true iff the two NotifierSignals are the same -- i.e., they combine
 * information about exactly the same sets of basic signals.
 *
 * All idle() signals compare equal.  busy_signal() and overderived_signal()
 * do not compare equal, however.
 */
inline bool
operator==(const NotifierSignal& a, const NotifierSignal& b)
{
    return (a._mask == b._mask && (a._value == b._value || a._mask == 0));
}

/** @relates NotifierSignal
 * @brief Compare two NotifierSignals for inequality.
 *
 * Returns true iff !(@a a == @a b).
 */
inline bool
operator!=(const NotifierSignal& a, const NotifierSignal& b)
{
    return !(a == b);
}

/** @relates NotifierSignal
 * @brief Return a derived signal.
 *
 * Returns a derived signal that combines information from its arguments.  The
 * result will be active whenever @a a and/or @a b is active.  If the
 * combination of @a a and @a b is too complex to represent, returns an
 * overderived signal; this trivially follows the invariant since it is always
 * active.
 *
 * Signal derivation is commutative and associative.  The following special
 * combinations are worth remembering:
 *
 *  - An idle() signal plus any other signal @a a equals @a a.  Thus,
 *    idle_signal() is the identity for signal derivation.
 *  - A busy() signal plus any other signal is busy().  Thus, busy_signal()
 *    is the "zero element" for signal derivation.
 *
 * @sa NotifierSignal::operator+=
 */
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

/** @brief Sets the associated signal's activity.
 * @param active true iff the signal should be active
 */
inline void
Notifier::set_active(bool active)
{
    _signal.set_active(active);
}   

/** @brief Sets the associated signal to active.
 * @sa set_active
 */
inline void
Notifier::wake()
{
    set_active(true);
}   

/** @brief Sets the associated signal to inactive.
 * @sa set_active
 */
inline void
Notifier::sleep()
{
    set_active(false);
}   

/** @brief Sets the associated signal's activity, possibly scheduling any
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
    if (active != Notifier::active()) {
	// 2007.Sep.6: Perhaps there was a race condition here.  Make sure
	// that we set the notifier to active BEFORE rescheduling downstream
	// tasks.  This is because, in a multithreaded environment, a task we
	// reschedule might run BEFORE we set the notifier; after which it
	// would go to sleep forever.
	Notifier::set_active(active);
	
	if (active && schedule) {
	    if (_listener1)
		_listener1->reschedule();
	    else if (task_or_signal_t *tos = _listeners) {
		for (; tos->t; tos++)
		    tos->t->reschedule();
		for (tos++; tos->s; tos++)
		    tos->s->set_active(true);
	    }
	}
    }
}

/** @brief Sets the associated signal to active and schedules any listener
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

/** @brief Sets the associated signal to inactive.
 * @sa set_active
 */
inline void
ActiveNotifier::sleep()
{
    set_active(false, true);
}

CLICK_ENDDECLS
#endif
