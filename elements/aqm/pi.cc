// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * pi.{cc,hh} -- element implements Random Early Detection dropping policy
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 ACIRI
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
#include "elements/standard/queue.hh"
#include <click/elemfilter.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#define PI_DEBUG 0

PI::PI()
    : Element(1, 1), _timer(this)
{
    MOD_INC_USE_COUNT;
}

PI::~PI()
{
    MOD_DEC_USE_COUNT;
}

PI *
PI::clone() const
{
    return new PI;
}

void
PI::notify_noutputs(int n)
{
    set_noutputs(n <= 1 ? 1 : 2);
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
PI::configure(const Vector<String> &conf, ErrorHandler *errh)
{
	double a, b, w;
	unsigned target_q;
    unsigned stability = 4;

    String queues_string = String();
	if (cp_va_parse(conf, this, errh,
	    cpDouble, "sampling frequency", &w,
	    cpDouble, "a", &a,
	    cpDouble, "b", &b,
		cpUnsigned, "target queue length", &target_q,
	    cpOptional,
	    cpArgument, "relevant queues", &queues_string,
	    cpKeywords,
	    "W", cpDouble, "sampling frequency", &w,
	    "A", cpDouble, "a", &a,
	    "B", cpDouble, "b", &b,
	    "QREF",  cpUnsigned, "target queue", &target_q,
	    "STABILITY", cpUnsigned, "stability shift", &stability,
	    "QUEUES", cpArgument, "relevant queues", &queues_string, 0) < 0)
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
PI::live_reconfigure(const Vector<String> &conf, ErrorHandler *errh)
{
	double a, b, w;
	unsigned target_q;
    unsigned stability = 4;

    String queues_string = String();
	if (cp_va_parse(conf, this, errh,
	    cpDouble, "sampling frequency", &w,
	    cpDouble, "a", &a,
	    cpDouble, "b", &b,
		cpUnsigned, "target queue length", &target_q,
	    cpOptional,
	    cpArgument, "relevant queues", &queues_string,
	    cpKeywords,
	    "W", cpDouble, "sampling frequency", &w,
	    "A", cpDouble, "a", &a,
	    "B", cpDouble, "b", &b,
	    "QREF",  cpUnsigned, "target queue", &target_q,
	    "STABILITY", cpUnsigned, "stability shift", &stability,
	    "QUEUES", cpArgument, "relevant queues", &queues_string, 0) < 0)
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
		CastElementFilter filter("Storage");

	int ok;
	if (output_is_push(0))
	    ok = router()->downstream_elements(this, 0, &filter, _queue_elements);
	else
	    ok = router()->upstream_elements(this, 0, &filter, _queue_elements);
	if (ok < 0)
	    return errh->error("flow-based router context failure");
	filter.filter(_queue_elements);
    }

    if (_queue_elements.size() == 0)
	return errh->error("no Queues downstream");
    for (int i = 0; i < _queue_elements.size(); i++)
	if (Storage *s = (Storage *)_queue_elements[i]->cast("Storage"))
	    _queues.push_back(s);
	else
	    errh->error("`%s' is not a Storage element", _queue_elements[i]->id().cc());
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
    _timer.schedule_after_ms(_w*1000);

    return 0;
}

void PI::uninitialize()
{
    _timer.uninitialize();
}

void
PI::take_state(Element *e, ErrorHandler *)
{
    PI *r = (PI *)e->cast("PI");
    if (!r) return;
    _size = r->_size;
}

void
PI::configuration(Vector<String> &conf, bool *) const
{
    conf.push_back(String(_a));
    conf.push_back(String(_b));
    conf.push_back(String(_w));
    conf.push_back(String(_target_q));

    StringAccum sa;
    sa << "QUEUES";
    for (int i = 0; i < _queue_elements.size(); i++)
	sa << ' ' << _queue_elements[i]->id();
    conf.push_back(sa.take_string());

    conf.push_back("STABILITY " + String(_size.stability_shift()));
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
PI::run_scheduled()
{
	_p = _a*(queue_size() - _target_q) - _b*(_old_q - _target_q) + _p;
    _timer.reschedule_after_ms(_w*1000);
}

bool
PI::should_drop()
{
	double _random_value = random();
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
    return String(r->drops()) + "\n";
}

String
PI::read_parameter(Element *f, void *vparam)
{
    PI *pi = (PI *)f;
    switch ((int)vparam) {
      case 0:			// w 
	return String(pi->_w) + "\n";
      case 1:			// a 
	return String(pi->_a) + "\n";
      case 2:			// b 
	return String(pi->_b) + "\n";
      case 3:			// _target_q 
	return String(pi->_target_q) + "\n";
      default:
	return "";
    }
}

String
PI::read_stats(Element *f, void *)
{
    PI *r = (PI *)f;
    return
	String(r->queue_size()) + " current queue\n" +
	cp_unparse_real2(r->_size.average(), QUEUE_SCALE) + " avg queue\n" +
	String(r->drops()) + " drops\n"
#if CLICK_STATS >= 1
	+ String(r->output(0).npackets()) + " packets\n"
#endif
	;
}

String
PI::read_queues(Element *e, void *)
{
    PI *r = (PI *)e;
    String s;
    for (int i = 0; i < r->_queue_elements.size(); i++)
	s += r->_queue_elements[i]->id() + "\n";
    return s;
}

void
PI::add_handlers()
{
    add_read_handler("drops", pi_read_drops, 0);
    add_read_handler("stats", read_stats, 0);
    add_read_handler("queues", read_queues, 0);
    add_read_handler("min_thresh", read_parameter, (void *)0);
    add_write_handler("min_thresh", reconfigure_positional_handler_2, (void *)0);
    add_read_handler("max_thresh", read_parameter, (void *)1);
    add_write_handler("max_thresh", reconfigure_positional_handler_2, (void *)1);
    add_read_handler("max_p", read_parameter, (void *)2);
    add_write_handler("max_p", reconfigure_positional_handler_2, (void *)2);
    add_read_handler("avg_queue_size", read_parameter, (void *)3);
}


ELEMENT_REQUIRES(Storage int64 false)
EXPORT_ELEMENT(PI)
