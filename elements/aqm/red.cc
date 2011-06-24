// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * red.{cc,hh} -- element implements Random Early Detection dropping policy
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2009 Intel Corporation
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
#include "red.hh"
#include <click/standard/storage.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/args.hh>
#include <click/straccum.hh>
CLICK_DECLS

#define RED_DEBUG 0

RED::RED()
{
}

RED::~RED()
{
}

void
RED::set_C1_and_C2()
{
    if (_min_thresh >= _max_thresh) {
	_C1 = 0;
	_C2 = 1;
    } else {
	_C1 = _max_p / (_max_thresh - _min_thresh);
	// There is no point in storing C2 as a scaled number, since there
	// is no 64-bit divide.
	_C2 = (_max_p * _min_thresh) / (_max_thresh - _min_thresh);
    }

    _G1 = (0x10000 - _max_p) / _max_thresh;
    _G2 = 0x10000 - 2*_max_p;
}

int
RED::check_params(unsigned min_thresh, unsigned max_thresh,
		  unsigned max_p, unsigned stability, ErrorHandler *errh) const
{
    unsigned max_allow_thresh = 0xFFFF;
    if (max_thresh > max_allow_thresh)
	return errh->error("MAX_THRESH must be <= %d", max_allow_thresh);
    if (min_thresh > max_thresh)
	return errh->error("MIN_THRESH must be <= MAX_THRESH");
    if (max_p > 0x10000)
	return errh->error("MAX_P must be between 0 and 1");
    if (stability > 16)
	return errh->error("STABILITY must be between 0 and 16");
    return 0;
}

int
RED::finish_configure(unsigned min_thresh, unsigned max_thresh, bool gentle,
		      unsigned max_p, unsigned stability,
		      const String &queues_string, ErrorHandler *errh)
{
    if (check_params(min_thresh, max_thresh, max_p, stability, errh) < 0)
	return -1;

    // check queues_string, but only if queues have not been configured already
    if (queues_string && !_queue_elements.size()) {
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
    _min_thresh = min_thresh;
    _max_thresh = max_thresh;
    _kill_thresh = gentle && max_p != 0x10000 ? max_thresh * 2 : max_thresh;
    _max_p = max_p;
    _size.set_stability_shift(stability);
    _gentle = gentle;
    set_C1_and_C2();
    return 0;
}

int
RED::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned min_thresh, max_thresh, max_p, stability = 4;
    String queues_string = String();
    bool gentle = true;
    if (Args(conf, this, errh)
	.read_mp("MIN_THRESH", min_thresh)
	.read_mp("MAX_THRESH", max_thresh)
	.read_mp("MAX_P", FixedPointArg(16), max_p)
	.read("QUEUES", AnyArg(), queues_string)
	.read("STABILITY", stability)
	.read("GENTLE", gentle)
	.complete() < 0)
	return -1;
    return finish_configure(min_thresh, max_thresh, gentle, max_p,
			    stability, queues_string, errh);
}

int
RED::initialize(ErrorHandler *errh)
{
    // Find the next queues
    _queues.clear();
    _queue1 = 0;

    if (_queue_elements.empty()) {
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

    if (_queue_elements.empty())
	return errh->error("no nearby Queues");
    for (int i = 0; i < _queue_elements.size(); i++)
	if (Storage *s = (Storage *)_queue_elements[i]->cast("Storage"))
	    _queues.push_back(s);
	else
	    errh->error("%<%s%> is not a Storage element", _queue_elements[i]->name().c_str());
    if (_queues.size() != _queue_elements.size())
	return -1;
    else if (_queues.size() == 1)
	_queue1 = _queues[0];

    _size.clear();
    _drops = 0;
    _count = -1;
    _last_jiffies = 0;
    return 0;
}

void
RED::take_state(Element *e, ErrorHandler *)
{
    if (RED *r = (RED *)e->cast("RED"))
	_size = r->_size;
}

int
RED::queue_size() const
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

bool
RED::should_drop()
{
    // calculate the new average queue size.
    // Do some rigamarole to handle empty periods, but don't work too hard.
    // (Therefore it contains errors. XXX)
    int s = queue_size();
    unsigned avg;

    if (_size.stability_shift() == 0)
	avg = s;		// use instantaneous measurement
    else if (s) {
	_size.update(s);
	_last_jiffies = 0;
	avg = _size.unscaled_average();
    } else {
	// do timing stuff for when the queue was empty
#if CLICK_HZ < 50
	click_jiffies_t j = click_jiffies();
#else
	click_jiffies_t j = click_jiffies() / (CLICK_HZ / 50);
#endif
	_size.update_n(0, _last_jiffies ? j - _last_jiffies : 1);
	_last_jiffies = j;
	avg = _size.unscaled_average();
    }

    if (avg <= _min_thresh) {
	_count = -1;
#if RED_DEBUG
	click_chatter("%s: no drop", declaration().c_str());
#endif
	return false;
    } else if (avg > _kill_thresh) {
	_count = -1;
#if RED_DEBUG
	click_chatter("%s: drop, over max_thresh", declaration().c_str());
#endif
	return true;
    }

    // note: use SCALED _size.average()
    int p_b;
    if (avg <= _max_thresh)
	p_b = ((_C1 * _size.scaled_average()) >> QUEUE_SCALE) - _C2;
    else
	p_b = ((_G1 * _size.scaled_average()) >> QUEUE_SCALE) - _G2;

    _count++;
    // note: division had Approx[]
    if (_count > 0 && p_b > 0 && _count > _random_value / p_b) {
#if RED_DEBUG
	click_chatter("%s: drop, random drop (%d, %d, %d, %d)", declaration().c_str(), _count, p_b, _random_value, _random_value/p_b);
#endif
	_count = 0;
	_random_value = (click_random() >> 5) & 0xFFFF;
	return true;
    }

    // otherwise, not dropping
    if (_count == 0)
	_random_value = (click_random() >> 5) & 0xFFFF;

#if RED_DEBUG
    click_chatter("%s: no drop", declaration().c_str());
#endif
    return false;
}

inline void
RED::handle_drop(Packet *p)
{
    if (noutputs() == 1)
	p->kill();
    else
	output(1).push(p);
    _drops++;
}

void
RED::push(int, Packet *p)
{
    if (should_drop())
	handle_drop(p);
    else
	output(0).push(p);
}

Packet *
RED::pull(int)
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

String
RED::read_handler(Element *f, void *vparam)
{
    RED *red = (RED *)f;
    StringAccum sa;
    switch ((intptr_t)vparam) {
      case 3:			// avg_queue_size
	return red->_size.unparse();
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
	    sa << red->_queue_elements[i]->name() << "\n";
	return sa.take_string();
      default:			// config
	sa << red->_min_thresh << ", " << red->_max_thresh << ", "
	   << cp_unparse_real2(red->_max_p, 16) << ", QUEUES";
	for (int i = 0; i < red->_queue_elements.size(); i++)
	    sa << ' ' << red->_queue_elements[i]->name();
	sa << ", STABILITY " << red->_size.stability_shift();
	if (!red->_gentle)
	    sa << ", GENTLE false";
	return sa.take_string();
    }
}

void
RED::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_read_handler("min_thresh", read_keyword_handler, "0 MIN_THRESH");
    add_write_handler("min_thresh", reconfigure_keyword_handler, "0 MIN_THRESH");
    add_read_handler("max_thresh", read_keyword_handler, "1 MAX_THRESH");
    add_write_handler("max_thresh", reconfigure_keyword_handler, "1 MAX_THRESH");
    add_read_handler("max_p", read_keyword_handler, "2 MAX_P");
    add_write_handler("max_p", reconfigure_keyword_handler, "2 MAX_P");
    add_read_handler("avg_queue_size", read_handler, 3);
    add_read_handler("stats", read_handler, 4);
    add_read_handler("queues", read_handler, 5);
    add_read_handler("config", read_handler, 6);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(int64)
EXPORT_ELEMENT(RED)
