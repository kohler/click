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
const char Notifier::FULL_NOTIFIER[] = "Notifier.FULL";

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

/** @class Notifier
 * @brief Class for notification providers.
 *
 * The Notifier class combines a basic activity signal with the optional
 * ability to wake up clients when the activity signal becomes active.  These
 * clients are called @e listeners.  Each listener corresponds to a Task
 * object.  The listener generally goes to sleep -- i.e., becomes unscheduled
 * -- when it runs out of work and the corresponding activity signal is
 * inactive.  The notifiers pledge to wake up each listener -- i.e., by
 * rescheduling the corresponding Task -- on becoming active.  The Notifier
 * class itself does not wake up clients; see ActiveNotifier for that.
 *
 * Elements that contain Notifier objects will generally override
 * Element::cast(), allowing other parts of the configuration to find the
 * Notifiers.  See upstream_empty_signal() and downstream_full_signal().
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


/** @brief Destruct a Notifier. */
Notifier::~Notifier()
{
}

/** @brief Called to register a listener with this Notifier.
 * @param task the listener's Task
 *
 * This notifier should register @a task as a listener, if appropriate.
 * Later, when the signal is activated, the Notifier should reschedule @a task
 * along with the other listeners.  Not all types of Notifier need to provide
 * this functionality, however.  The default implementation does nothing.
 */
int
Notifier::add_listener(Task* task)
{
    (void) task;
    return 0;
}

/** @brief Called to unregister a listener with this Notifier.
 * @param task the listener's Task
 *
 * Undoes the effect of any prior add_listener(@a task).  Should do nothing if
 * @a task was never added.  The default implementation does nothing.
 */
void
Notifier::remove_listener(Task* task)
{
    (void) task;
}

/** @brief Initialize the associated NotifierSignal, if necessary.
 * @param r the associated router
 *
 * Initialize the Notifier's associated NotifierSignal by calling @a r's
 * Router::new_notifier_signal() method, obtaining a new basic activity
 * signal.  Does nothing if the signal is already initialized.
 */
int
Notifier::initialize(Router* r)
{
    if (!_signal.initialized())
	return r->new_notifier_signal(_signal);
    else
	return 0;
}


ActiveNotifier::ActiveNotifier(SearchOp search_op)
    : Notifier(search_op), _listener1(0), _listeners(0)
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
	if (!n->signal().initialized())
	    n->initialize(e->router());
	_signal += n->signal();
	Notifier::SearchOp search_op = n->search_op();
	if (search_op == Notifier::SEARCH_CONTINUE_WAKE) {
	    _need_pass2 = true;
	    return !_pass2;
	} else
	    return search_op == Notifier::SEARCH_STOP;
	
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

/** @brief Calculate and return the NotifierSignal derived from all empty
 * notifiers upstream of element @a e's input @a port, and optionally register
 * @a task as a listener.
 * @param e an element
 * @param port the input port of @a e at which to start the upstream search
 * @param task Task to register as a listener, or null
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
 * A nonnull @a task argument is added to each located notifier as a listener.
 * Thus, when a notifier becomes active (when packets become available), the
 * @a task will be rescheduled.
 *
 * <h3>Supporting upstream_empty_signal()</h3>
 *
 * Elements that have an empty notifier must override the Element::cast()
 * method.  When passed the @a name Notifier::EMPTY_NOTIFIER, this method
 * should return a pointer to the corresponding Notifier object.
 */
NotifierSignal
Notifier::upstream_empty_signal(Element* e, int port, Task* task)
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

    if (task)
	for (int i = 0; i < filter._notifiers.size(); i++)
	    filter._notifiers[i]->add_listener(task);

    return signal;
}

/** @brief Calculate and return the NotifierSignal derived from all full
 * notifiers downstream of element @a e's output @a port, and optionally
 * register @a task as a listener.
 * @param e an element
 * @param port the output port of @a e at which to start the downstream search
 * @param task Task to register as a listener, or null
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
 * A nonnull @a task argument is added to each located notifier as a listener.
 * Thus, when a notifier becomes active (when space become available), the @a
 * task will be rescheduled.
 *
 * <h3>Supporting downstream_full_signal()</h3>
 *
 * Elements that have a full notifier must override the Element::cast()
 * method.  When passed the @a name Notifier::FULL_NOTIFIER, this method
 * should return a pointer to the corresponding Notifier object.
 */
NotifierSignal
Notifier::downstream_full_signal(Element* e, int port, Task* task)
{
    NotifierElementFilter filter(FULL_NOTIFIER);
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

    if (task)
	for (int i = 0; i < filter._notifiers.size(); i++)
	    filter._notifiers[i]->add_listener(task);

    return signal;
}

CLICK_ENDDECLS
