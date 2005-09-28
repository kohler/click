// -*- c-basic-offset: 4; related-file-name: "../include/click/notifier.hh" -*-
/*
 * notifier.{cc,hh} -- activity notification
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004-2005 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/notifier.hh>
#include <click/router.hh>
#include <click/element.hh>
#include <click/elemfilter.hh>
#include <click/bitvector.hh>
CLICK_DECLS

// should be const, but we need to explicitly initialize it
atomic_uint32_t NotifierSignal::static_value;
const char Notifier::EMPTY_NOTIFIER[] = "Notifier.EMPTY";
const char Notifier::NONFULL_NOTIFIER[] = "Notifier.NONFULL";

/** @class NotifierSignal
 * @brief Represents an activity signal.
 *
 * Activity signals in Click let one element determine whether another element
 * is active.  For example, consider an element @e X pulling from a @e Queue.
 * If the @e Queue is empty, there's no point in @e X trying to pull from it.
 * Thus, the @e Queue has an activity signal that's active when it contains
 * packets and inactive when it's empty.  @e X can check the activity signal
 * before pulling, and do something else if it's inactive.  Combined with the
 * sleep/wakeup functionality of ActiveNotifier, this can greatly reduce CPU
 * load due to polling.
 *
 * A "basic activity signal" is essentially a bit that's either on or off.
 * When it's on, the signal is active.  NotifierSignal can represent @e
 * derived activity signals as well.  A derived signal combines information
 * about @e N basic signals using the following invariant: If any of the basic
 * signals is active, then the derived signal is also active.  There are no
 * other guarantees; in particular, the derived signal might be active even if
 * @e none of the basic signals are active.
 *
 * Click elements construct NotifierSignal objects in four ways:
 *
 *  - idle_signal() returns a signal that's never active.
 *  - busy_signal() returns a signal that's always active.
 *  - Router::new_notifier_signal() creates a new basic signal.  This method
 *    should be preferred to NotifierSignal's own constructors.
 *  - operator+(NotifierSignal, const NotifierSignal &) creates a derived signal.
 */

/** @brief Initialize the NotifierSignal implementation.
 *
 * This function must be called before NotifierSignal functionality is used.
 * It is safe to call it multiple times.
 *
 * @note Elements don't need to worry about static_initialize(); Click drivers
 * have already called it for you.
 */
void
NotifierSignal::static_initialize()
{
    static_value = TRUE_MASK | OVERDERIVED_MASK;
}

/** @brief Make this signal derived by adding information from @a a.
 * @param a the signal to add
 *
 * Creates a derived signal that combines information from this signal and @a
 * a.  Equivalent to "*this = (*this + @a a)".
 *
 * @sa operator+(NotifierSignal, const NotifierSignal&)
 */
NotifierSignal&
NotifierSignal::operator+=(const NotifierSignal& a)
{
    if (!_mask)
	_value = a._value;

    // preserve busy_signal(); adding other incompatible signals
    // leads to overderived_signal()
    if (*this == busy_signal())
	/* do nothing */;
    else if (a == busy_signal())
	*this = a;
    else if (_value == a._value || !a._mask)
	_mask |= a._mask;
    else
	*this = overderived_signal();
    
    return *this;
}

/** @brief Return a human-readable representation of the signal.
 *
 * Only useful for signal debugging.
 */
String
NotifierSignal::unparse() const
{
    char buf[40];
    sprintf(buf, "%p/%x:%x", _value, _mask, (*_value)&_mask);
    return String(buf);
}


Notifier::~Notifier()
{
}

NotifierSignal
Notifier::notifier_signal()
{
    return NotifierSignal::idle_signal();
}

Notifier::SearchOp
Notifier::notifier_search_op()
{
    return SEARCH_DONE;
}

int
Notifier::add_listener(Task*)
{
    return 0;
}

void
Notifier::remove_listener(Task*)
{
}


int
PassiveNotifier::initialize(Router* r)
{
    if (_signal == NotifierSignal())
	return r->new_notifier_signal(_signal);
    else
	return 0;
}

NotifierSignal
PassiveNotifier::notifier_signal()
{
    return _signal;
}


ActiveNotifier::ActiveNotifier()
    : _listener1(0), _listeners(0)
{
}

ActiveNotifier::~ActiveNotifier()
{
    delete[] _listeners;
}

int
ActiveNotifier::add_listener(Task* new_l)
{
    if (!_listeners && (!_listener1 || _listener1 == new_l)) {
	_listener1 = new_l;
	return 0;
    } else if (new_l == 0)
	return 0;

    int n = (_listener1 ? 1 : 0);
    if (!_listener1)
	for (Task **t = _listeners; *t; t++) {
	    if (*t == new_l)
		return 0;
	    n++;
	}
    Task** old_list = (_listener1 ? &_listener1 : _listeners);
    
    Task** new_list = new Task *[n + 2];
    if (!new_list) {
	click_chatter("out of memory in Notifier::add_listener!");
	return -1;
    }
    memcpy(new_list, old_list, sizeof(Task *) * 1);
    new_list[n] = new_l;
    new_list[n + 1] = _listener1 = 0;
    delete[] _listeners;
    _listeners = new_list;
    return 0;
}

void
ActiveNotifier::remove_listener(Task* bad_l)
{
    if (!bad_l)
	/* nada */;
    else if (_listener1 == bad_l)
	_listener1 = 0;
    else if (_listeners) {
	int n = 0, which = -1;
	for (Task** l = _listeners; *l; l++) {
	    if (*l == bad_l)
		which = n;
	    n++;
	}
	if (which >= 0) {
	    _listeners[which] = _listeners[n - 1];
	    _listeners[n - 1] = 0;
	}
    }
}

void
ActiveNotifier::listeners(Vector<Task*>& v) const
{
    if (_listener1)
	v.push_back(_listener1);
    else if (_listeners)
	for (Task** l = _listeners; *l; l++)
	    v.push_back(*l);
}


namespace {

class NotifierElementFilter : public ElementFilter { public:
    NotifierElementFilter(const char* name);
    bool check_match(Element*, int, PortType);
    Vector<Notifier*> _notifiers;
    NotifierSignal _signal;
    bool _pass2;
    bool _need_pass2;
    const char* _name;
};

NotifierElementFilter::NotifierElementFilter(const char* name)
    : _signal(NotifierSignal::idle_signal()),
      _pass2(false), _need_pass2(false), _name(name)
{
}

bool
NotifierElementFilter::check_match(Element* e, int port, PortType pt)
{
    if (Notifier* n = (Notifier*) (e->cast(_name))) {
	_notifiers.push_back(n);
	_signal += n->notifier_signal();
	Notifier::SearchOp search_op = n->notifier_search_op();
	if (search_op == Notifier::SEARCH_WAKE_CONTINUE) {
	    _need_pass2 = true;
	    return !_pass2;
	} else
	    return search_op == Notifier::SEARCH_DONE;
	
    } else if (pt != NONE) {
	Bitvector flow;
	if (e->port_active(pt, port) // went from pull <-> push
	    || (e->port_flow(pt, port, &flow), flow.zero())) {
	    _signal = NotifierSignal::busy_signal();
	    return true;
	} else
	    return false;

    } else
	return false;
}

}


NotifierSignal
Notifier::upstream_empty_signal(Element* e, int port, Task* t)
{
    NotifierElementFilter filter(EMPTY_NOTIFIER);
    Vector<Element*> v;
    int ok = e->router()->upstream_elements(e, port, &filter, v);

    NotifierSignal signal = filter._signal;

    // maybe run another pass
    if (ok >= 0 && signal != NotifierSignal() && filter._need_pass2) {
	filter._pass2 = true;
	ok = e->router()->upstream_elements(e, port, &filter, v);
    }
    
    // All bets are off if filter ran into a push output. That means there was
    // a regular Queue in the way (for example).
    if (ok < 0 || signal == NotifierSignal())
	return NotifierSignal();

    if (t)
	for (int i = 0; i < filter._notifiers.size(); i++)
	    filter._notifiers[i]->add_listener(t);

    return signal;
}

NotifierSignal
Notifier::downstream_nonfull_signal(Element* e, int port, Task* t)
{
    NotifierElementFilter filter(NONFULL_NOTIFIER);
    Vector<Element*> v;
    int ok = e->router()->downstream_elements(e, port, &filter, v);

    NotifierSignal signal = filter._signal;

    // maybe run another pass
    if (ok >= 0 && signal != NotifierSignal() && filter._need_pass2) {
	filter._pass2 = true;
	ok = e->router()->downstream_elements(e, port, &filter, v);
    }
    
    // All bets are off if filter ran into a pull input. That means there was
    // a regular Queue in the way (for example).
    if (ok < 0 || signal == NotifierSignal())
	return NotifierSignal();

    if (t)
	for (int i = 0; i < filter._notifiers.size(); i++)
	    filter._notifiers[i]->add_listener(t);

    return signal;
}

CLICK_ENDDECLS
