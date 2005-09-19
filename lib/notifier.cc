// -*- c-basic-offset: 4; related-file-name: "../include/click/notifier.hh" -*-
/*
 * notifier.{cc,hh} -- activity notification
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
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

void
NotifierSignal::static_initialize()
{
    static_value = TRUE_MASK | TRUE_CONFLICT_MASK;
}

NotifierSignal&
NotifierSignal::operator+=(const NotifierSignal& o)
{
    if (!_mask)
	_value = o._value;

    // preserve always_active_signal(); adding other incompatible signals
    // leads to conflicted_signal()
    if (*this == always_active_signal())
	/* do nothing */;
    else if (o == always_active_signal())
	*this = o;
    else if (_value == o._value || !o._mask)
	_mask |= o._mask;
    else
	*this = conflicted_signal(true);
    
    return *this;
}

String
NotifierSignal::unparse() const
{
    char buf[40];
    sprintf(buf, "%p/%x:%x", _value, _mask, (*_value)&_mask);
    return String(buf);
}


NotifierSignal
Notifier::notifier_signal()
{
    return NotifierSignal::always_inactive_signal();
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
    : _signal(NotifierSignal::empty_signal()),
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
	    _signal = NotifierSignal::always_active_signal();
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
