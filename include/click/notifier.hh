// -*- c-basic-offset: 4; related-file-name: "../../lib/notifier.cc" -*-
#ifndef CLICK_NOTIFIER_HH
#define CLICK_NOTIFIER_HH
#include <click/task.hh>
#include <click/atomic.hh>
CLICK_DECLS

class NotifierSignal { public:

    inline NotifierSignal();		// always active
    inline NotifierSignal(bool);
    inline NotifierSignal(atomic_uint32_t *value, uint32_t mask);
    static void static_initialize();

    static inline NotifierSignal always_active_signal();
    static inline NotifierSignal always_inactive_signal();
    static inline NotifierSignal conflicted_signal();
    
    bool active() const			{ return (*_value & _mask) != 0; }
    operator bool() const		{ return active(); }

    inline bool always_active() const;
    inline bool conflicted() const;

    inline void set_active(bool a);

    NotifierSignal &operator+=(const NotifierSignal &);

    String unparse() const;
    
  private:

    atomic_uint32_t *_value;
    uint32_t _mask;

    enum { TRUE_MASK = 1, CONFLICT_MASK = 2 };
    static atomic_uint32_t true_value;
    friend bool operator==(const NotifierSignal &, const NotifierSignal &);
    friend bool operator!=(const NotifierSignal &, const NotifierSignal &);

};

class Notifier { public:

    Notifier()			{ }
    virtual ~Notifier()		{ }

    virtual NotifierSignal notifier_signal();
    
    enum SearchOp { SEARCH_DONE = 0, SEARCH_CONTINUE, SEARCH_WAKE_CONTINUE };
    virtual SearchOp notifier_search_op();
    
    virtual int add_listener(Task *);
    virtual void remove_listener(Task *);

    static const char * const EMPTY_NOTIFIER;
    
    static NotifierSignal upstream_empty_signal(Element *, int port, Task *);
    
};

class PassiveNotifier : public Notifier { public:

    PassiveNotifier()			{ }

    int initialize(Router *);
    
    NotifierSignal notifier_signal();

    bool signal_active() const		{ return _signal.active(); }
    void set_signal_active(bool active)	{ _signal.set_active(active); }

  private:

    NotifierSignal _signal;

    friend class ActiveNotifier;
    
};

class ActiveNotifier : public PassiveNotifier { public:

    ActiveNotifier();
    ~ActiveNotifier()			{ delete[] _listeners; }

    int add_listener(Task *);		// complains on out of memory
    void remove_listener(Task *);
    void listeners(Vector<Task *> &) const;

    void wake_listeners();
    void sleep_listeners();
    void set_listeners(bool awake);
    
  private:
    
    Task *_listener1;
    Task **_listeners;

    ActiveNotifier(const ActiveNotifier &); // does not exist
    ActiveNotifier &operator=(const ActiveNotifier &); // does not exist

};


inline
NotifierSignal::NotifierSignal()
    : _value(&true_value), _mask(TRUE_MASK)
{
}

inline
NotifierSignal::NotifierSignal(bool always_on)
    : _value(&true_value), _mask(always_on ? TRUE_MASK : 0)
{
}

inline
NotifierSignal::NotifierSignal(atomic_uint32_t *value, uint32_t mask)
    : _value(value), _mask(mask)
{
}

inline NotifierSignal
NotifierSignal::always_active_signal()
{
    return NotifierSignal(&true_value, TRUE_MASK);
}

inline NotifierSignal
NotifierSignal::always_inactive_signal()
{
    return NotifierSignal(&true_value, 0);
}

inline NotifierSignal
NotifierSignal::conflicted_signal()
{
    return NotifierSignal(&true_value, CONFLICT_MASK);
}

inline bool
NotifierSignal::always_active() const
{
    return (_value == &true_value && _mask != 0);
}

inline bool
NotifierSignal::conflicted() const
{
    return (_value == &true_value && _mask == CONFLICT_MASK);
}

inline void
NotifierSignal::set_active(bool b)
{
    if (b)
	*_value |= _mask;
    else
	*_value &= ~_mask;
}

inline bool
operator==(const NotifierSignal &a, const NotifierSignal &b)
{
    return (a._mask == b._mask && (a._value == b._value || a._mask == 0));
}

inline bool
operator!=(const NotifierSignal &a, const NotifierSignal &b)
{
    return !(a == b);
}

inline NotifierSignal
operator+(NotifierSignal a, const NotifierSignal &b)
{
    return a += b;
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
