// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ACTIVITY_HH
#define CLICK_ACTIVITY_HH
#include <click/task.hh>

class ActivitySignal { public:

    ActivitySignal();		// always true
    ActivitySignal(volatile uint32_t *value, uint32_t mask);

    bool active() const			{ return (*_value & _mask) != 0; }

    void set_active(bool a);

    ActivitySignal &operator+=(const ActivitySignal &);

  private:

    volatile uint32_t *_value;
    uint32_t _mask;

    static uint32_t true_value;

};

class ActivityNotifier { public:

    ActivityNotifier();
    ~ActivityNotifier()		{ delete[] _listeners; }

    int initialize(Router *);
    
    int add_listener(Task *);	// complains on out of memory
    void remove_listener(Task *);

    static ActivitySignal listen_upstream_pull(Element *, int port, Task *);

    bool listeners_awake() const	{ return _signal.active(); }
    bool listeners_asleep() const	{ return !_signal.active(); }
    void wake_listeners();
    void sleep_listeners();

    const ActivitySignal &activity_signal() const { return _signal; }
    
  private:
    
    Task *_listener1;
    Task **_listeners;
    ActivitySignal _signal;

};


inline
ActivitySignal::ActivitySignal()
    : _value(&true_value), _mask(1)
{
}

inline
ActivitySignal::ActivitySignal(volatile uint32_t *value, uint32_t mask)
    : _value(value), _mask(mask)
{
}

inline void
ActivitySignal::set_active(bool b)
{
    if (b)
	*_value |= _mask;
    else
	*_value = (*_value & ~_mask);
}

inline ActivitySignal &
ActivitySignal::operator+=(const ActivitySignal &o)
{
    if (_value == o._value)
	_mask |= o._mask;
    else
	_value = &true_value;
    return *this;
}

inline ActivitySignal
operator+(ActivitySignal a, const ActivitySignal &b)
{
    return a += b;
}

inline void
ActivityNotifier::sleep_listeners()
{
    _signal.set_active(false);
}

inline void
ActivityNotifier::wake_listeners()
{
    if (_listener1)
	_listener1->reschedule();
    else if (_listeners)
	for (Task **t = _listeners; *t; t++)
	    (*t)->reschedule();
    _signal.set_active(true);
}

#endif
