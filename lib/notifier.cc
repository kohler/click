// -*- c-basic-offset: 4; related-file-name: "../include/click/notifier.hh" -*-
/*
 * notifier.{cc,hh} -- activity notification
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004-2005 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
 * Copyright (c) 2012 Eddie Kohler
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
#if HAVE_CXX_PRAGMA_INTERFACE
# pragma implementation "click/notifier.hh"
#endif
#include <click/notifier.hh>
#include <click/router.hh>
#include <click/element.hh>
#include <click/routervisitor.hh>
#include <click/straccum.hh>
#include <click/bitvector.hh>
CLICK_DECLS

// should be const, but we need to explicitly initialize it
const char Notifier::EMPTY_NOTIFIER[] = "empty";
const char Notifier::FULL_NOTIFIER[] = "full";

/** @file notifier.hh
 * @brief Support for activity signals.
 */

/** @class NotifierSignal
 * @brief An activity signal.
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
 * @brief A basic activity signal and notification provider.
 *
 * The Notifier class represents a basic activity signal associated with an
 * element.  Elements that contain a Notifier object will override
 * Element::cast() or Element::port_cast() to return that Notifier when given
 * the proper name.  This lets other parts of the configuration find the
 * Notifiers.  See upstream_empty_signal() and downstream_full_signal().
 *
 * The ActiveNotifier class, which derives from Notifier, can wake up clients
 * when its activity signal becomes active.
 */

/** @class ActiveNotifier
 * @brief A basic activity signal and notification provider that can
 * reschedule any dependent Task objects.
 *
 * ActiveNotifier, whose base class is Notifier, combines a basic activity
 * signal with the ability to wake up any dependent Task objects when that
 * signal becomes active.  Notifier clients are called @e listeners.  Each
 * listener corresponds to a Task object.  The listener generally goes to
 * sleep -- i.e., becomes unscheduled -- when it runs out of work and the
 * corresponding activity signal is inactive.  The ActiveNotifier class will
 * wake up the listener when it becomes active by rescheduling the relevant
 * Task.
 *
 * Elements that contain ActiveNotifier objects will generally override
 * Element::cast() or Element::port_cast(), allowing other parts of the
 * configuration to find the Notifiers.
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
}

NotifierSignal &
NotifierSignal::operator+=(const NotifierSignal &x)
{
    // preserve busy_signal(); adding other incompatible signals
    // leads to overderived_signal()
    if (idle() || (x.busy() && *this != busy_signal()) || !x.initialized())
	*this = x;
    else if (busy() || !initialized() || x.idle() || _v.v == x._v.v)
	/* do nothing */;
    else if (*x._v.v != 2)
	hard_derive_one(x._v.v);
    else
	for (value_type **vm = x._v.vp + 1; *vm; ++vm)
	    hard_derive_one(*vm);

    return *this;
}

void
NotifierSignal::hard_assign_vm(const NotifierSignal &x)
{
    size_t n = 1;
    for (value_type **vm = x._v.vp + 1; *vm; ++vm)
	++n;
    if (likely((_v.vp = new value_type *[n + 1])))
	memcpy(_v.vp, x._v.vp, sizeof(*_v.vp) * (n + 1));
    else
	// cannot call "*this = overderived_signal()" b/c _v is invalid
	_v.u = overderived_value;
}

void
NotifierSignal::hard_derive_one(value_type *value)
{
    if (unlikely(*_v.v != 2)) {
	if (busy() || unlikely(value == _v.v))
	    return;
	value_type **vmp;
	if (unlikely(!(vmp = new value_type *[4]))) {
	    *this = overderived_signal();
	    return;
	}
	vmp[1] = _v.v < value ? _v.v : value;
	vmp[2] = _v.v < value ? value : _v.v;
	vmp[3] = 0;
	_v.vp = vmp;
	*_v.v = 2;
	return;
    }

    size_t i, n;
    value_type **vmp;
    for (i = 1, vmp = _v.vp + 1; *vmp && *vmp < value; ++i, ++vmp)
	/* do nothing */;
    if (*vmp == value)
	return;
    for (n = i; *vmp; ++n, ++vmp)
	/* do nothing */;

    if (unlikely(!(vmp = new value_type *[n + 2]))) {
	*this = overderived_signal();
	return;
    }
    memcpy(vmp, _v.vp, sizeof(*_v.vp) * i);
    vmp[i] = value;
    memcpy(vmp + i + 1, _v.vp + i, sizeof(*_v.vp) * (n + 1 - i));
    delete[] _v.vp;
    _v.vp = vmp;
}

bool
NotifierSignal::hard_equals(value_type **a, value_type **b)
{
    while (*a && *a == *b)
	++a, ++b;
    return !*a && !*b;
}

String
NotifierSignal::unparse(Router *router) const
{
    if (!is_static() && *_v.v == 2) {
	StringAccum sa;
	for (value_type **vm = _v.vp + 1; *vm; ++vm)
	    sa << (vm == _v.vp + 1 ? "" : "+")
	       << NotifierSignal(*vm).unparse(router);
	return sa.take_string();
    }

    char buf[80];
    int pos;
    String s;
    if (is_static()) {
	if (busy() && !overderived())
	    return "busy*";
	else if (idle())
	    return "idle";
	else if (overderived())
	    return "overderived*";
	else if (!initialized())
	    return "uninitialized";
	else
	    pos = sprintf(buf, "internal%d", (int) _v.u);
    } else if (router && (s = router->notifier_signal_name(_v.v)) >= 0) {
	pos = sprintf(buf, "%.52s", s.c_str());
    } else
	pos = sprintf(buf, "@%p", _v.v);
    if (active())
	buf[pos++] = '*';
    return String(buf, pos);
}


/** @brief Destruct a Notifier. */
Notifier::~Notifier()
{
}

void
Notifier::dependent_signal_callback(void *user_data, Notifier *)
{
    NotifierSignal *signal = static_cast<NotifierSignal *>(user_data);
    signal->set_active(true);
}

/** @brief Register an activate callback with this Notifier.
 * @param f callback function
 * @param user_data callback data for @a f
 * @return 1 if notifier was added, 0 on other success, negative on error
 *
 * When this Notifier's associated signal is activated, this Notifier should
 * call @a f(@a user_data, this). Not all types of Notifier provide this
 * functionality. The default implementation does nothing.
 *
 * If @a f is null, then @a user_data is a Task pointer passed to
 * add_listener.
 *
 * @sa remove_activate_callback, add_listener, add_dependent_signal
 */
int
Notifier::add_activate_callback(callback_type f, void *user_data)
{
    (void) f, (void) user_data;
    return 0;
}

/** @brief Unregister an activate callback with this Notifier.
 * @param f callback function
 * @param user_data callback data for @a f
 *
 * Undoes the effect of all prior add_activate_callback(@a f, @a user_data)
 * calls. Does nothing if (@a f,@a user_data) was never added. The default
 * implementation does nothing.
 *
 * If @a f is null, then @a user_data is a Task pointer passed to
 * remove_listener.
 *
 * @sa add_activate_callback
 */
void
Notifier::remove_activate_callback(callback_type f, void *user_data)
{
    (void) f, (void) user_data;
}

/** @brief Initialize the associated NotifierSignal, if necessary.
 * @param name signal name
 * @param r associated router
 *
 * Initialize the Notifier's associated NotifierSignal by calling @a r's
 * Router::new_notifier_signal() method, obtaining a new basic activity
 * signal.  Does nothing if the signal is already initialized.
 */
int
Notifier::initialize(const char *name, Router *r)
{
    if (!_signal.initialized())
	return r->new_notifier_signal(name, _signal);
    else
	return 0;
}


/** @brief Construct an ActiveNotifier.
 * @param op controls notifier path search
 *
 * Constructs an ActiveNotifier object, analogous to the
 * Notifier::Notifier(SearchOp) constructor.  (See that constructor for more
 * information on @a op.)
 */
ActiveNotifier::ActiveNotifier(SearchOp op)
    : Notifier(op), _listener1(0), _listeners(0)
{
}

/** @brief Destroy an ActiveNotifier. */
ActiveNotifier::~ActiveNotifier()
{
    delete[] _listeners;
}

int
ActiveNotifier::add_activate_callback(callback_type f, void *v)
{
    // common case
    if (!_listener1 && !_listeners && !f) {
	_listener1 = static_cast<Task *>(v);
	return 1;
    }

    // count existing listeners
    int delta = 1;
    task_or_signal_t *tos = _listeners;
    for (; tos && tos->p; tos += delta)
	if (tos->p == 1) {
	    delta = 2;
	    --tos;
	} else if ((!f && delta == 1 && tos->t == static_cast<Task *>(v))
		   || (f && delta == 2 && tos->f == f && tos[1].v == v))
	    return 0;

    // create new listener array
    int n = tos - _listeners + 1;
    if (_listener1)
	++n;
    if (f)
	n += (delta == 2 ? 2 : 3);
    else
	++n;
    task_or_signal_t *ntos = new task_or_signal_t[n];
    if (!ntos) {
	click_chatter("out of memory in Notifier!");
	return -ENOMEM;
    }

    // populate listener array
    task_or_signal_t *otos = ntos;
    if (_listener1)
	(otos++)->t = _listener1;
    for (tos = _listeners; tos && tos->p > 1; ++tos)
	(otos++)->t = tos->t;
    if (!f)
	(otos++)->t = static_cast<Task *>(v);
    if (f || (tos && tos->p == 1)) {
	(otos++)->p = 1;
	if (tos && tos->p == 1)
	    for (++tos; tos->p > 0; ) {
		*otos++ = *tos++;
		*otos++ = *tos++;
	    }
	if (f) {
	    (otos++)->f = f;
	    (otos++)->v = v;
	}
    }
    (otos++)->p = 0;

    delete[] _listeners;
    _listeners = ntos;
    _listener1 = 0;
    return 1;
}

void
ActiveNotifier::remove_activate_callback(callback_type f, void *v)
{
    if (!f && _listener1 == static_cast<Task *>(v)) {
	_listener1 = 0;
	return;
    }
    int delta = 0, step = 1;
    task_or_signal_t *tos;
    for (tos = _listeners; tos && tos->p; )
	if ((!f && step == 1 && tos->t == static_cast<Task *>(v))
	    || (f && step == 2 && tos->f == f && tos[1].v == v)) {
	    delta = -step;
	    tos += step;
	} else {
	    if (delta)
		tos[delta] = *tos;
	    if (delta && step == 2)
		tos[delta + 1] = tos[1];
	    if (tos->p == 1) {
		++tos;
		step = 2;
	    } else
		tos += step;
	}
    if (delta != 0)
	tos[delta].p = 0;
}

/** @brief Return the listener list.
 * @param[out] v collects listener tasks
 *
 * Pushes all listener Task objects onto the end of @a v.
 */
void
ActiveNotifier::listeners(Vector<Task*>& v) const
{
    if (_listener1)
	v.push_back(_listener1);
    else if (_listeners)
	for (task_or_signal_t* l = _listeners; l->p > 1; ++l)
	    v.push_back(l->t);
}

#if CLICK_DEBUG_SCHEDULING
String
ActiveNotifier::unparse(Router *router) const
{
    StringAccum sa;
    sa << signal().unparse(router) << '\n';
    if (_listener1 || _listeners)
	for (int i = 0; _listener1 ? i == 0 : _listeners[i].p > 1; ++i) {
	    Task *t = _listener1 ? _listener1 : _listeners[i].t;
	    sa << "task " << ((void *) t) << ' ';
	    if (Element *e = t->element())
		sa << '[' << e->declaration() << "] ";
	    sa << (t->scheduled() ? "scheduled\n" : "unscheduled\n");
	}
    return sa.take_string();
}
#endif


namespace {

class NotifierRouterVisitor : public RouterVisitor { public:
    NotifierRouterVisitor(const char* name);
    bool visit(Element *e, bool isoutput, int port,
	       Element *from_e, int from_port, int distance);
    Vector<Notifier*> _notifiers;
    NotifierSignal _signal;
    bool _pass2;
    bool _need_pass2;
    const char* _name;
};

NotifierRouterVisitor::NotifierRouterVisitor(const char* name)
    : _signal(NotifierSignal::idle_signal()),
      _pass2(false), _need_pass2(false), _name(name)
{
}

bool
NotifierRouterVisitor::visit(Element* e, bool isoutput, int port,
			     Element *, int, int)
{
    if (Notifier* n = (Notifier*) (e->port_cast(isoutput, port, _name))) {
	if (find(_notifiers.begin(), _notifiers.end(), n) == _notifiers.end())
	    _notifiers.push_back(n);
	if (!n->signal().initialized())
	    n->initialize(_name, e->router());
	_signal += n->signal();
	Notifier::SearchOp search_op = n->search_op();
	if (search_op == Notifier::SEARCH_CONTINUE_WAKE && !_pass2) {
	    _need_pass2 = true;
	    return false;
	} else
	    return search_op != Notifier::SEARCH_STOP;

    } else if (port >= 0) {
	Bitvector flow;
	if (e->port_active(isoutput, port)) {
	    // went from pull <-> push
	    _signal = NotifierSignal::busy_signal();
	    return false;
	} else if ((e->port_flow(isoutput, port, &flow), flow.zero())
		   && e->flag_value('S') != 0) {
	    // ran out of ports, but element might generate packets
	    _signal = NotifierSignal::busy_signal();
	    return false;
	} else
	    return true;

    } else
	return true;
}

}

/** @brief Calculate and return the NotifierSignal derived from all empty
 * notifiers upstream of element @a e's input @a port.
 * @param e an element
 * @param port the input port of @a e at which to start the upstream search
 * @param f callback function
 * @param user_data user data for callback function
 * @sa add_activate_callback */
NotifierSignal
Notifier::upstream_empty_signal(Element* e, int port, callback_type f, void *user_data)
{
    NotifierRouterVisitor filter(EMPTY_NOTIFIER);
    int ok = e->router()->visit_upstream(e, port, &filter);

    NotifierSignal signal = filter._signal;

    // maybe run another pass
    if (ok >= 0 && signal != NotifierSignal() && filter._need_pass2) {
	filter._pass2 = true;
	ok = e->router()->visit_upstream(e, port, &filter);
    }

    // All bets are off if filter ran into a push output. That means there was
    // a regular Queue in the way (for example).
    if (ok < 0 || signal == NotifierSignal())
	return NotifierSignal();

    if (f || user_data)
	for (int i = 0; i < filter._notifiers.size(); i++)
	    filter._notifiers[i]->add_activate_callback(f, user_data);

    return signal;
}

/** @brief Calculate and return the NotifierSignal derived from all full
 * notifiers downstream of element @a e's output @a port.
 * @param e an element
 * @param port the output port of @a e at which to start the downstream search
 * @param f callback function
 * @param user_data user data for callback function
 * @sa add_activate_callback */
NotifierSignal
Notifier::downstream_full_signal(Element* e, int port, callback_type f, void *user_data)
{
    NotifierRouterVisitor filter(FULL_NOTIFIER);
    int ok = e->router()->visit_downstream(e, port, &filter);

    NotifierSignal signal = filter._signal;

    // maybe run another pass
    if (ok >= 0 && signal != NotifierSignal() && filter._need_pass2) {
	filter._pass2 = true;
	ok = e->router()->visit_downstream(e, port, &filter);
    }

    // All bets are off if filter ran into a pull input. That means there was
    // a regular Queue in the way (for example).
    if (ok < 0 || signal == NotifierSignal())
	return NotifierSignal();

    if (f || user_data)
	for (int i = 0; i < filter._notifiers.size(); i++)
	    filter._notifiers[i]->add_activate_callback(f, user_data);

    return signal;
}

CLICK_ENDDECLS
