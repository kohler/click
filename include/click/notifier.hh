// -*- c-basic-offset: 4; related-file-name: "../../lib/notifier.cc" -*-
#ifndef CLICK_NOTIFIER_HH
#define CLICK_NOTIFIER_HH
#include <click/task.hh>

class NotifierSignal { public:

    NotifierSignal();			// always true
    NotifierSignal(bool);
    NotifierSignal(volatile uint32_t *value, uint32_t mask);

    bool active() const			{ return (*_value & _mask) != 0; }
    operator bool() const		{ return active(); }

    void set_active(bool a);

    NotifierSignal &operator+=(const NotifierSignal &);

  private:

    volatile uint32_t *_value;
    uint32_t _mask;

    static uint32_t true_value;

};

class Notifier { public:

    Notifier();
    ~Notifier()				{ delete[] _listeners; }

    int initialize(Router *);
    
    int add_listener(Task *);		// complains on out of memory
    void remove_listener(Task *);

    static NotifierSignal upstream_pull_signal(Element *, int port, Task* = 0);

    bool listeners_awake() const	{ return _signal.active(); }
    bool listeners_asleep() const	{ return !_signal.active(); }
    void wake_listeners();
    void sleep_listeners();

    const NotifierSignal &notifier_signal() const { return _signal; }
    
  private:
    
    Task *_listener1;
    Task **_listeners;
    NotifierSignal _signal;

};


inline
NotifierSignal::NotifierSignal()
    : _value(&true_value), _mask(1)
{
}

inline
NotifierSignal::NotifierSignal(bool always_on)
    : _value(&true_value), _mask(always_on)
{
}

inline
NotifierSignal::NotifierSignal(volatile uint32_t *value, uint32_t mask)
    : _value(value), _mask(mask)
{
}

inline void
NotifierSignal::set_active(bool b)
{
    if (b)
	*_value |= _mask;
    else
	*_value = (*_value & ~_mask);
}

inline NotifierSignal &
NotifierSignal::operator+=(const NotifierSignal &o)
{
    if (!_mask)
	_value = o._value;
    if (_value == o._value || !o._mask)
	_mask |= o._mask;
    else
	_value = &true_value;
    return *this;
}

inline NotifierSignal
operator+(NotifierSignal a, const NotifierSignal &b)
{
    return a += b;
}

inline void
Notifier::sleep_listeners()
{
    _signal.set_active(false);
}

inline void
Notifier::wake_listeners()
{
    if (_listener1)
	_listener1->reschedule();
    else if (_listeners)
	for (Task **t = _listeners; *t; t++)
	    (*t)->reschedule();
    _signal.set_active(true);
}

#endif
