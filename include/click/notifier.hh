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
    
    inline operator bool() const;
    inline bool active() const;

    inline bool idle() const;
    inline bool busy() const;
    inline bool overderived() const;

    inline void set_active(bool active);

    NotifierSignal& operator+=(const NotifierSignal& a);

    String unparse() const;
    
    static void static_initialize();
    
  private:

    atomic_uint32_t* _value;
    uint32_t _mask;

    enum { TRUE_MASK = 1, FALSE_MASK = 2, OVERDERIVED_MASK = 4 };
    static atomic_uint32_t static_value;
    friend bool operator==(const NotifierSignal&, const NotifierSignal&);
    friend bool operator!=(const NotifierSignal&, const NotifierSignal&);
    friend NotifierSignal operator+(NotifierSignal, const NotifierSignal&);

};

class Notifier { public:

    inline Notifier();
    virtual ~Notifier();

    virtual NotifierSignal notifier_signal();
    
    enum SearchOp { SEARCH_DONE = 0, SEARCH_CONTINUE, SEARCH_WAKE_CONTINUE };
    virtual SearchOp notifier_search_op();
    
    virtual int add_listener(Task*);
    virtual void remove_listener(Task*);

    static const char EMPTY_NOTIFIER[];
    static const char NONFULL_NOTIFIER[];
    
    static NotifierSignal upstream_empty_signal(Element*, int port, Task*);
    static NotifierSignal downstream_nonfull_signal(Element*, int port, Task*);
    
};

class PassiveNotifier : public Notifier { public:

    inline PassiveNotifier();

    int initialize(Router*);
    
    NotifierSignal notifier_signal();

    inline bool signal_active() const;
    inline void set_signal_active(bool active);

  private:

    NotifierSignal _signal;

    friend class ActiveNotifier;
    
};

class ActiveNotifier : public PassiveNotifier { public:

    ActiveNotifier();
    ~ActiveNotifier();

    int add_listener(Task*);		// complains on out of memory
    void remove_listener(Task*);
    void listeners(Vector<Task*>&) const;

    inline void wake_listeners();
    inline void sleep_listeners();
    inline void set_listeners(bool awake);
    
  private:
    
    Task* _listener1;
    Task** _listeners;

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
NotifierSignal::operator bool() const
{
    return active();
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
 */
inline bool
NotifierSignal::busy() const
{
    return (_value == &static_value && (_mask & TRUE_MASK));
}

/** @brief Return whether the signal is overderived.
 * @return true iff the signal equals overderived_signal().
 */
inline bool
NotifierSignal::overderived() const
{
    return (_value == &static_value && (_mask & OVERDERIVED_MASK));
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
 * All idle() signals compare equal.  busy() and overderived() signals do not
 * compare equal, however.
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
 * The following special signal combinations are worth remembering:
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

inline
Notifier::Notifier()
{
}

inline
PassiveNotifier::PassiveNotifier()
{
}

inline bool
PassiveNotifier::signal_active() const
{
    return _signal.active();
}

inline void
PassiveNotifier::set_signal_active(bool active)
{
    _signal.set_active(active);
}

inline void
ActiveNotifier::wake_listeners()
{
    if (_listener1)
	_listener1->reschedule();
    else if (_listeners)
	for (Task **t = _listeners; *t; t++)
	    (*t)->reschedule();
    _signal.set_active(true);
}

inline void
ActiveNotifier::sleep_listeners()
{
    _signal.set_active(false);
}

inline void
ActiveNotifier::set_listeners(bool awake)
{
    if (awake && !_signal.active()) {
	if (_listener1)
	    _listener1->reschedule();
	else if (_listeners)
	    for (Task **t = _listeners; *t; t++)
		(*t)->reschedule();
    }
    _signal.set_active(awake);
}

CLICK_ENDDECLS
#endif
