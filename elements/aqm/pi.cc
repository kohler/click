// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * pi.{cc,hh} -- element implements Random Early Detection dropping policy
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
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
#include "pi.hh"
#include <click/standard/storage.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/args.hh>
#include <click/straccum.hh>
CLICK_DECLS

#define PI_DEBUG 0

PI::PI()
    : _timer(this)
{
}

PI::~PI()
{
}

int
PI::check_params(double w, double a, double b, unsigned target_q,
					unsigned stability, ErrorHandler *errh) const
{
    unsigned max_allow_thresh = 0xFFFF;
	if (target_q > max_allow_thresh)
		return errh->error("`target_q' too large (max %d)", max_allow_thresh);
	if (w < 0)
		return errh->error("w must be positive");
	if (a < 0)
		return errh->error("a must be positive");
	if (b < 0)
		return errh->error("b must be positive");
    if (stability > 16 || stability < 1)
		return errh->error("STABILITY parameter must be between 1 and 16");
	return 0;
}

int
PI::configure(Vector<String> &conf, ErrorHandler *errh)
{
    double a, b, w;
    unsigned target_q;
    unsigned stability = 4;

    String queues_string = String();
    if (Args(conf, this, errh)
	.read_mp("W", w)
	.read_mp("A", a)
	.read_mp("B", b)
	.read_mp("TARGET", target_q)
	.read_p("QUEUES", AnyArg(), queues_string)
	.read("QREF", target_q)
	.read("STABILITY", stability)
	.complete() < 0)
	return -1;

    if (check_params(w, a, b, target_q, stability, errh) < 0)
		return -1;

    // check queues_string
    if (queues_string) {
		Vector<String> eids;
		cp_spacevec(queues_string, eids);
		_queue_elements.clear();
	for (int i = 0; i < eids.size(); i++)
		if (Element *e = router()->find(eids[i], this, errh))
			_queue_elements.push_back(e);
		if (eids.size() != _queue_elements.size())
		return -1;
    }

    // OK: set variables
	_a = a;
	_b = b;
	_w = w;
	_target_q = target_q;
    _size.set_stability_shift(stability);
    return 0;
}

int
PI::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
    double a, b, w;
    unsigned target_q;
    unsigned stability = 4;

    String queues_string = String();
    if (Args(conf, this, errh)
	.read_mp("W", w)
	.read_mp("A", a)
	.read_mp("B", b)
	.read_mp("TARGET", target_q)
	.read_p("QUEUES", AnyArg(), queues_string)
	.read("QREF", target_q)
	.read("STABILITY", stability)
	.complete() < 0)
	return -1;

    if (check_params(w, a, b, target_q, stability, errh) < 0)
		return -1;

    if (queues_string)
		errh->warning("QUEUES argument ignored");

    // OK: set variables
	_a = a;
	_b = b;
	_w = w;
	_target_q = target_q;
    _size.set_stability_shift(stability);
    return 0;
}

int
PI::initialize(ErrorHandler *errh)
{
    // Find the next queues
    _queues.clear();
    _queue1 = 0;

    if (!_queue_elements.size()) {
	ElementCastTracker filter(router(), "Storage");

	int ok;
	if (output_is_push(0))
	    ok = router()->visit_downstream(this, 0, &filter);
	else
	    ok = router()->visit_upstream(this, 0, &filter);
	if (ok < 0)
	    return errh->error("flow-based router context failure");
	_queue_elements = filter.elements();
    }

    if (_queue_elements.size() == 0)
	return errh->error("no Queues downstream");
    for (int i = 0; i < _queue_elements.size(); i++)
	if (Storage *s = (Storage *)_queue_elements[i]->cast("Storage"))
	    _queues.push_back(s);
	else
	    errh->error("`%s' is not a Storage element", _queue_elements[i]->name().c_str());
    if (_queues.size() != _queue_elements.size())
	return -1;
    else if (_queues.size() == 1)
	_queue1 = _queues[0];

    _size.clear();
	_old_q = 0;
	_p = 0;
    _drops = 0;
    _last_jiffies = 0;

    _timer.initialize(this);
    _timer.schedule_after_msec(_w*1000);

    return 0;
}

void PI::cleanup(CleanupStage)
{
    _timer.cleanup();
}

void
PI::take_state(Element *e, ErrorHandler *)
{
    PI *r = (PI *)e->cast("PI");
    if (!r) return;
    _size = r->_size;
}

int
PI::queue_size() const
{
    if (_queue1)
	return _queue1->size();
    else {
	int s = 0;
	for (int i = 0; i < _queues.size(); i++)
	    s += _queues[i]->size();
	return s;
    }
}

void
PI::run_timer(Timer *)
{
	_p = _a*(queue_size() - _target_q) - _b*(_old_q - _target_q) + _p;
    _timer.reschedule_after_msec(_w*1000);
}

bool
PI::should_drop()
{
	double _random_value = click_random();
    if (_random_value > _p*MAX_RAND) {
		return true;
    }
    return false;
}

inline void
PI::handle_drop(Packet *p)
{
    if (noutputs() == 1)
	p->kill();
    else
	output(1).push(p);
    _drops++;
}

void
PI::push(int, Packet *p)
{
    if (should_drop())
	handle_drop(p);
    else
	output(0).push(p);
}

Packet *
PI::pull(int)
{
    while (true) {
	Packet *p = input(0).pull();
	if (!p)
	    return 0;
	else if (!should_drop())
	    return p;
	handle_drop(p);
    }
}


// HANDLERS

static String
pi_read_drops(Element *f, void *)
{
    PI *r = (PI *)f;
    return String(r->drops());
}

String
PI::read_parameter(Element *f, void *vparam)
{
    PI *pi = (PI *)f;
    StringAccum sa;
    switch ((int)vparam) {
      case 3:			// _target_q
	return String(pi->_target_q);
      case 4:			// stats
	sa << red->queue_size() << " current queue\n"
	   << red->_size.unparse() << " avg queue\n"
	   << red->drops() << " drops\n"
#if CLICK_STATS >= 1
	   << red->output(0).npackets() << " packets\n"
#endif
	    ;
	return sa.take_string();
      case 5:			// queues
	for (int i = 0; i < red->_queue_elements.size(); i++)
	    sa << red->_queue_elements[i]->name() + "\n";
	return sa.take_string();
      default:
	sa << _a << ", " << _b << ", " << _w << ", " << _target_q
	   << ", QUEUES";
	for (int i = 0; i < _queue_elements.size(); i++)
	    sa << ' ' << _queue_elements[i]->name();
	sa << ", STABILITY " << _size.stability_shift();
	return sa.take_string();
    }
}

void
PI::add_handlers()
{
    add_read_handler("drops", pi_read_drops);
    set_handler("w", Handler::OP_READ | Handler::OP_WRITE, configuration_keyword_handler, "W", (void *) (uintptr_t) 1);
    set_handler("a", Handler::OP_READ | Handler::OP_WRITE, configuration_keyword_handler, "A", (void *) (uintptr_t) 2);
    set_handler("b", Handler::OP_READ | Handler::OP_WRITE, configuration_keyword_handler, "B", (void *) (uintptr_t) 3);
    add_read_handler("avg_queue_size", read_parameter, 3);
    add_read_handler("stats", read_parameter, 4);
    add_read_handler("queues", read_parameter, 5);
    add_read_handler("config", read_parameter, 6);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(int64 false)
EXPORT_ELEMENT(PI)
