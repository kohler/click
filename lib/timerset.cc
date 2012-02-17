// -*- c-basic-offset: 4; related-file-name: "../include/click/timerset.hh" -*-
/*
 * timerset.{cc,hh} -- Click set of timers
 * Eddie Kohler
 *
 * Copyright (c) 2003-7 The Regents of the University of California
 * Copyright (c) 2010 Intel Corporation
 * Copyright (c) 2008-2010 Meraki, Inc.
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
#include <click/timerset.hh>
#include <click/task.hh>
#include <click/routerthread.hh>
#include <click/heap.hh>
#include <click/master.hh>
CLICK_DECLS

TimerSet::TimerSet()
{
#if CLICK_NS
    _max_timer_stride = 1;
#else
    _max_timer_stride = 32;
#endif
    _timer_stride = _max_timer_stride;
    _timer_count = 0;
#if CLICK_LINUXMODULE
    _timer_check_reports = 5;
#else
    _timer_check_reports = 0;
#endif

#if CLICK_LINUXMODULE
    _timer_task = 0;
#elif HAVE_MULTITHREAD
    _timer_processor = click_invalid_processor();
#endif
    _timer_check = Timestamp::now_steady();
    _timer_check_reports = 0;
}

void
TimerSet::kill_router(Router *router)
{
    lock_timers();
    assert(!_timer_runchunk.size());
    for (heap_element *thp = _timer_heap.end();
	 thp > _timer_heap.begin(); ) {
	--thp;
	Timer *t = thp->t;
	if (t->router() == router) {
	    remove_heap<4>(_timer_heap.begin(), _timer_heap.end(), thp, heap_less(), heap_place());
	    _timer_heap.pop_back();
	    t->_owner = 0;
	    t->_schedpos1 = 0;
	}
    }
    set_timer_expiry();
    unlock_timers();
}

void
TimerSet::set_max_timer_stride(unsigned timer_stride)
{
    _max_timer_stride = timer_stride;
    if (_timer_stride > _max_timer_stride)
	_timer_stride = _max_timer_stride;
}

void
TimerSet::check_timer_expiry(Timer *t)
{
    // do not schedule timers for too far in the past
    if (t->_expiry_s.sec() + Timer::behind_sec < _timer_check.sec()) {
	if (_timer_check_reports > 0) {
	    --_timer_check_reports;
	    click_chatter("timer %p outdated expiry %p{timestamp} updated to %p{timestamp}", t, &t->_expiry_s, &_timer_check);
	}
	t->_expiry_s = _timer_check;
    }
}

inline void
TimerSet::run_one_timer(Timer *t)
{
#if CLICK_STATS >= 2
    Element *owner = t->_owner;
    click_cycles_t start_cycles = click_get_cycles(),
	start_child_cycles = owner->_child_cycles;
#endif

    t->_hook.callback(t, t->_thunk);

#if CLICK_STATS >= 2
    click_cycles_t all_delta = click_get_cycles() - start_cycles,
	own_delta = all_delta - (owner->_child_cycles - start_child_cycles);
    owner->_timer_calls += 1;
    owner->_timer_own_cycles += own_delta;
#endif
}

void
TimerSet::run_timers(RouterThread *thread, Master *master)
{
    if (!_timer_lock.attempt())
	return;
    if (!master->paused() && _timer_heap.size() > 0 && !thread->stop_flag()) {
	thread->set_thread_state(RouterThread::S_RUNTIMER);
#if CLICK_LINUXMODULE
	_timer_task = current;
#elif HAVE_MULTITHREAD
	_timer_processor = click_current_processor();
#endif
	_timer_check = Timestamp::now_steady();
	heap_element *th = _timer_heap.begin();

	if (th->expiry_s <= _timer_check) {
	    // potentially adjust timer stride
	    Timestamp adj_expiry = th->expiry_s + Timer::adjustment();
	    if (adj_expiry <= _timer_check) {
		_timer_count = 0;
		if (_timer_stride > 1)
		    _timer_stride = (_timer_stride * 4) / 5;
	    } else if (++_timer_count >= 12) {
		_timer_count = 0;
		if (++_timer_stride >= _max_timer_stride)
		    _timer_stride = _max_timer_stride;
	    }

	    // actually run timers
	    int max_timers = 64;
	    do {
		Timer *t = th->t;
		assert(t->expiry_steady() == th->expiry_s);
		pop_heap<4>(_timer_heap.begin(), _timer_heap.end(), heap_less(), heap_place());
		_timer_heap.pop_back();
		set_timer_expiry();
		t->_schedpos1 = 0;

		run_one_timer(t);
	    } while (_timer_heap.size() > 0 && !thread->stop_flag()
		     && (th = _timer_heap.begin(), th->expiry_s <= _timer_check)
		     && --max_timers >= 0);

	    // If we ran out of timers to run, then perhaps there's an
	    // infinite timer loop or one timer is very far behind system
	    // time.  Eventually the system would catch up and run all timers,
	    // but in the meantime other timers could starve.  We detect this
	    // case and run ALL expired timers, reducing possible damage.
	    if (max_timers < 0 && !thread->stop_flag()) {
		_timer_runchunk.reserve(32);
		do {
		    Timer *t = th->t;
		    pop_heap<4>(_timer_heap.begin(), _timer_heap.end(), heap_less(), heap_place());
		    _timer_heap.pop_back();
		    t->_schedpos1 = -_timer_runchunk.size() - 1;

		    _timer_runchunk.push_back(t);
		} while (_timer_heap.size() > 0
			 && (th = _timer_heap.begin(), th->expiry_s <= _timer_check));
		set_timer_expiry();

		Vector<Timer*>::iterator i = _timer_runchunk.begin();
		for (; !thread->stop_flag() && i != _timer_runchunk.end(); ++i)
		    if (*i) {
			(*i)->_schedpos1 = 0;
			run_one_timer(*i);
		    }

		// reschedule unrun timers if stopped early
		for (; i != _timer_runchunk.end(); ++i)
		    if (*i) {
			(*i)->_schedpos1 = 0;
			(*i)->schedule_at_steady((*i)->_expiry_s);
		    }
		_timer_runchunk.clear();
	    }
	}

#if CLICK_LINUXMODULE
	_timer_task = 0;
#elif HAVE_MULTITHREAD
	_timer_processor = click_invalid_processor();
#endif
    }
    _timer_lock.release();
}

CLICK_ENDDECLS
