// -*- c-basic-offset: 4; related-file-name: "../../lib/notifier.cc" -*-
#ifndef CLICK_NOTIFIER_HH
#define CLICK_NOTIFIER_HH
#include <click/task.hh>
#include <click/atomic.hh>
CLICK_DECLS

class NotifierSignal { public:

    inline NotifierSignal();		// always active
    inline NotifierSignal(atomic_uint32_t* value, uint32_t mask);
    static void static_initialize();

    static inline NotifierSignal empty_signal();
    static inline NotifierSignal always_active_signal();
    static inline NotifierSignal always_inactive_signal();
    static inline NotifierSignal conflicted_signal(bool active);
    
    bool active() const			{ return (*_value & _mask) != 0; }
    bool all_active() const		{ return (*_value & _mask) == _mask; }
    operator bool() const		{ return active(); }

    bool empty() const			{ return !_mask; }
    inline bool always_active() const;
    inline bool conflicted() const;

    inline void set_active(bool a);

    NotifierSignal& operator+=(const NotifierSignal&);

    String unparse() const;
    
  private:

    atomic_uint32_t* _value;
    uint32_t _mask;

    enum { TRUE_MASK = 1, FALSE_MASK = 2, TRUE_CONFLICT_MASK = 4, FALSE_CONFLICT_MASK = 8 };
    static atomic_uint32_t static_value;
    friend bool operator==(const NotifierSignal&, const NotifierSignal&);
    friend bool operator!=(const NotifierSignal&, const NotifierSignal&);

};

class Notifier { public:

    Notifier()			{ }
    virtual ~Notifier()		{ }

    virtual NotifierSignal notifier_signal();
    
    enum SearchOp { SEARCH_DONE = 0, SEARCH_CONTINUE, SEARCH_WAKE_CONTINUE };
    virtual SearchOp notifier_search_op();
    
    virtual int add_listener(Task*);
    virtual void remove_listener(Task*);

    static const char* const EMPTY_NOTIFIER;
    static const char* const NONFULL_NOTIFIER;
    
    static NotifierSignal upstream_empty_signal(Element*, int port, Task*);
    static NotifierSignal downstream_nonfull_signal(Element*, int port, Task*);
    
};

class PassiveNotifier : public Notifier { public:

    PassiveNotifier()			{ }

    int initialize(Router*);
    
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

    int add_listener(Task*);		// complains on out of memory
    void remove_listener(Task*);
    void listeners(Vector<Task*>&) const;

    void wake_listeners();
    void sleep_listeners();
    void set_listeners(bool awake);
    
  private:
    
    Task* _listener1;
    Task** _listeners;

    ActiveNotifier(const ActiveNotifier&); // does not exist
    ActiveNotifier& operator=(const ActiveNotifier&); // does not exist

};


inline
NotifierSignal::NotifierSignal()
    : _value(&static_value), _mask(TRUE_MASK)
{
}

inline
NotifierSignal::NotifierSignal(atomic_uint32_t* value, uint32_t mask)
    : _value(value), _mask(mask)
{
}

inline NotifierSignal
NotifierSignal::empty_signal()
{
    return NotifierSignal(&static_value, 0);
}

inline NotifierSignal
NotifierSignal::always_active_signal()
{
    return NotifierSignal(&static_value, TRUE_MASK);
}

inline NotifierSignal
NotifierSignal::always_inactive_signal()
{
    return NotifierSignal(&static_value, 0);
}

inline NotifierSignal
NotifierSignal::conflicted_signal(bool active)
{
    return NotifierSignal(&static_value, active ? TRUE_CONFLICT_MASK | TRUE_MASK : FALSE_CONFLICT_MASK | FALSE_MASK);
}

inline bool
NotifierSignal::always_active() const
{
    return (_value == &static_value && (_mask & TRUE_MASK));
}

inline bool
NotifierSignal::conflicted() const
{
    return (_value == &static_value && (_mask & (TRUE_CONFLICT_MASK | FALSE_CONFLICT_MASK)));
}

inline void
NotifierSignal::set_active(bool b)
{
    assert(_value != &static_value);
    if (b)
	*_value |= _mask;
    else
	*_value &= ~_mask;
}

inline bool
operator==(const NotifierSignal& a, const NotifierSignal& b)
{
    return (a._mask == b._mask && (a._value == b._value || a._mask == 0));
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
