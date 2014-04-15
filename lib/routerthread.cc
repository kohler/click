// -*- c-basic-offset: 4; related-file-name: "../include/click/routerthread.hh" -*-
/*
 * routerthread.{cc,hh} -- Click threads
 * Eddie Kohler, Benjie Chen, Petros Zerfos
 *
 * Copyright (c) 2000-2001 Massachusetts Institute of Technology
 * Copyright (c) 2001-2002 International Computer Science Institute
 * Copyright (c) 2004-2007 Regents of the University of California
 * Copyright (c) 2008-2010 Meraki, Inc.
 * Copyright (c) 2000-2012 Eddie Kohler
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
#include <click/glue.hh>
#include <click/router.hh>
#include <click/routerthread.hh>
#include <click/master.hh>
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/sched.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
#if CLICK_BSDMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <sys/kthread.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#elif CLICK_USERLEVEL
# include <fcntl.h>
#endif
CLICK_DECLS

#define DEBUG_RT_SCHED		0

#define PROFILE_ELEMENT		20

#if HAVE_ADAPTIVE_SCHEDULER
# define DRIVER_TOTAL_TICKETS	128	/* # tickets shared between clients */
# define DRIVER_GLOBAL_STRIDE	(Task::STRIDE1 / DRIVER_TOTAL_TICKETS)
# define DRIVER_QUANTUM		8	/* microseconds per stride */
# define DRIVER_RESTRIDE_INTERVAL 80	/* microseconds between restrides */
#endif

#if CLICK_LINUXMODULE
static unsigned long greedy_schedule_jiffies;
#endif

/** @file routerthread.hh
 * @brief The RouterThread class implementing the Click driver loop.
 */

/** @class RouterThread
 * @brief A set of Tasks scheduled on the same CPU.
 */

RouterThread::RouterThread(Master *master, int id)
    : _stop_flag(0), _master(master), _id(id)
{
    _pending_head.x = 0;
    _pending_tail = &_pending_head;

#if !HAVE_TASK_HEAP
    _task_link._prev = _task_link._next = &_task_link;
#endif
#if CLICK_LINUXMODULE
    _linux_task = 0;
#elif CLICK_USERLEVEL && HAVE_MULTITHREAD
    _running_processor = click_invalid_processor();
#endif

    _task_blocker = 0;
    _task_blocker_waiting = 0;
#if HAVE_ADAPTIVE_SCHEDULER
    _max_click_share = 80 * Task::MAX_UTILIZATION / 100;
    _min_click_share = Task::MAX_UTILIZATION / 200;
    _cur_click_share = 0;	// because we aren't yet running
#endif

#if CLICK_NS
    _tasks_per_iter = 256;
#else
#ifdef BSD_NETISRSCHED
    // Must be set low for Luigi's feedback scheduler to work properly
    _tasks_per_iter = 8;
#else
    _tasks_per_iter = 128;
#endif
#endif

    _iters_per_os = 2;		// userlevel: iterations per select()
				// kernel: iterations per OS schedule()

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
    _greedy = false;
#endif
#if CLICK_LINUXMODULE
    greedy_schedule_jiffies = jiffies;
#endif

#if CLICK_NS
    _ns_scheduled = _ns_last_active = Timestamp(-1, 0);
    _ns_active_iter = 0;
#endif

#if CLICK_DEBUG_SCHEDULING
    _thread_state = S_BLOCKED;
    _driver_epoch = 0;
    _driver_task_epoch = 0;
    _task_epoch_first = 0;
# if CLICK_DEBUG_SCHEDULING > 1
    for (int s = 0; s < NSTATES; ++s)
	_thread_state_count[s] = 0;
# endif
#endif

    static_assert(THREAD_QUIESCENT == (int) ThreadSched::THREAD_QUIESCENT
		  && THREAD_UNKNOWN == (int) ThreadSched::THREAD_UNKNOWN,
		  "Thread constants screwup.");
}

RouterThread::~RouterThread()
{
    assert(!active());
}

inline void
RouterThread::driver_lock_tasks()
{
    set_thread_state(S_LOCKTASKS);

    // If other people are waiting for the task lock, give them a chance to
    // catch it before we claim it.
#if CLICK_LINUXMODULE
    for (int i = 0; _task_blocker_waiting > 0 && i < 10; i++)
	schedule();
#elif HAVE_MULTITHREAD && CLICK_USERLEVEL
    for (int i = 0; _task_blocker_waiting > 0 && i < 10; i++) {
	struct timeval waiter = { 0, 1 };
	select(0, 0, 0, 0, &waiter);
    }
#endif

    while (_task_blocker.compare_swap(0, (uint32_t) -1) != 0) {
#if CLICK_LINUXMODULE
	schedule();
#endif
    }
}

inline void
RouterThread::driver_unlock_tasks()
{
    uint32_t val = _task_blocker.compare_swap((uint32_t) -1, 0);
    (void) val;
    assert(val == (uint32_t) -1);
}

void
RouterThread::scheduled_tasks(Router *router, Vector<Task *> &x)
{
    lock_tasks();
    for (Task *t = task_begin(); t != task_end(); t = task_next(t))
	if (t->router() == router)
	    x.push_back(t);
    unlock_tasks();
}

void
RouterThread::request_stop()
{
    _stop_flag = 1;
    if (current_thread_is_running()) {
	// Set the current thread's tasks to "is_scheduled 2" and mark them as
	// pending.  As a result the driver loop will not run these tasks:
	// it's checking for "is_scheduled 1".  (We cannot call
	// Task::remove_from_scheduled_list(), because run_tasks keeps the
	// current task in limbo, so it must stay in the scheduled list.)
	Task::Status want_status;
	want_status.home_thread_id = thread_id();
	want_status.is_scheduled = true;
	want_status.is_strong_unscheduled = false;
	Task::Status new_status(want_status);
	new_status.is_strong_unscheduled = 2;

	for (Task *t = task_begin(); t != task_end(); t = task_next(t))
	    if (atomic_uint32_t::compare_swap(t->_status.status, want_status.status, new_status.status) == want_status.status)
		t->add_pending();
    }
}


/******************************/
/* Adaptive scheduler         */
/******************************/

#if HAVE_ADAPTIVE_SCHEDULER

void
RouterThread::set_cpu_share(unsigned min_frac, unsigned max_frac)
{
    if (min_frac == 0)
	min_frac = 1;
    if (min_frac > Task::MAX_UTILIZATION - 1)
	min_frac = Task::MAX_UTILIZATION - 1;
    if (max_frac > Task::MAX_UTILIZATION - 1)
	max_frac = Task::MAX_UTILIZATION - 1;
    if (max_frac < min_frac)
	max_frac = min_frac;
    _min_click_share = min_frac;
    _max_click_share = max_frac;
}

void
RouterThread::client_set_tickets(int client, int new_tickets)
{
    Client &c = _clients[client];

    // pin 'tickets' in a reasonable range
    if (new_tickets < 1)
	new_tickets = 1;
    else if (new_tickets > Task::MAX_TICKETS)
	new_tickets = Task::MAX_TICKETS;
    unsigned new_stride = Task::STRIDE1 / new_tickets;
    assert(new_stride < Task::MAX_STRIDE);

    // calculate new pass, based possibly on old pass
    // start with a full stride on initialization (c.tickets == 0)
    if (c.tickets == 0)
	c.pass = _global_pass + new_stride;
    else {
	int delta = (c.pass - _global_pass) * new_stride / c.stride;
	c.pass = _global_pass + delta;
    }

    c.tickets = new_tickets;
    c.stride = new_stride;
}

inline void
RouterThread::client_update_pass(int client, const Timestamp &t_before)
{
    Client &c = _clients[client];
    Timestamp t_now = Timestamp::now();
    Timestamp::value_type elapsed = (t_now - t_before).usecval();
    if (elapsed > 0)
	c.pass += (c.stride * elapsed) / DRIVER_QUANTUM;
    else
	c.pass += c.stride;

    // check_restride
    elapsed = (t_now - _adaptive_restride_timestamp).usecval();
    if (elapsed > DRIVER_RESTRIDE_INTERVAL || elapsed < 0) {
	// mark new measurement period
	_adaptive_restride_timestamp = t_now;

	// reset passes every 10 intervals, or when time moves backwards
	if (++_adaptive_restride_iter == 10 || elapsed < 0) {
	    _global_pass = _clients[C_CLICK].tickets = _clients[C_KERNEL].tickets = 0;
	    _adaptive_restride_iter = 0;
	} else
	    _global_pass += (DRIVER_GLOBAL_STRIDE * elapsed) / DRIVER_QUANTUM;

	// find out the maximum amount of work any task performed
	int click_utilization = 0;
	Task *end = task_end();
	for (Task *t = task_begin(); t != end; t = task_next(t)) {
	    int u = t->utilization();
	    t->clear_runs();
	    if (u > click_utilization)
		click_utilization = u;
	}

	// constrain to bounds
	if (click_utilization < _min_click_share)
	    click_utilization = _min_click_share;
	if (click_utilization > _max_click_share)
	    click_utilization = _max_click_share;

	// set tickets
	int click_tix = (DRIVER_TOTAL_TICKETS * click_utilization) / Task::MAX_UTILIZATION;
	if (click_tix < 1)
	    click_tix = 1;
	client_set_tickets(C_CLICK, click_tix);
	client_set_tickets(C_KERNEL, DRIVER_TOTAL_TICKETS - _clients[C_CLICK].tickets);
	_cur_click_share = click_utilization;
    }
}

#endif

/******************************/
/* Debugging                  */
/******************************/

#if CLICK_DEBUG_SCHEDULING
Timestamp
RouterThread::task_epoch_time(uint32_t epoch) const
{
    if (epoch >= _task_epoch_first && epoch <= _driver_task_epoch)
	return _task_epoch_time[epoch - _task_epoch_first];
    else if (epoch > _driver_task_epoch - TASK_EPOCH_BUFSIZ && epoch <= _task_epoch_first - 1)
	// "-1" makes this code work even if _task_epoch overflows
	return _task_epoch_time[epoch - (_task_epoch_first - TASK_EPOCH_BUFSIZ)];
    else
	return Timestamp();
}
#endif


/******************************/
/* The driver loop            */
/******************************/

#if HAVE_TASK_HEAP
#define PASS_GE(a, b)	((int)(a - b) >= 0)

void
RouterThread::task_reheapify_from(int pos, Task* t)
{
    // MUST be called with task lock held
    task_heap_element *tbegin = _task_heap.begin();
    task_heap_element *tend = _task_heap.end();
    int npos;

    while (pos > 0
	   && (npos = (pos-1) >> 1, PASS_GT(tbegin[npos].pass, t->_pass))) {
	tbegin[pos] = tbegin[npos];
	tbegin[npos].t->_schedpos = pos;
	pos = npos;
    }

    while (1) {
	Task *smallest = t;
	task_heap_element *tp = tbegin + 2*pos + 1;
	if (tp < tend && PASS_GE(smallest->_pass, tp[0].pass))
	    smallest = tp[0].t;
	if (tp + 1 < tend && PASS_GE(smallest->_pass, tp[1].pass))
	    smallest = tp[1].t, ++tp;

	smallest->_schedpos = pos;
	tbegin[pos].t = smallest;
	tbegin[pos].pass = smallest->_pass;

	if (smallest == t)
	    return;

	pos = tp - tbegin;
    }
}
#endif

/* Run at most 'ntasks' tasks. */
inline void
RouterThread::run_tasks(int ntasks)
{
    set_thread_state(S_RUNTASK);
#if CLICK_DEBUG_SCHEDULING
    _driver_task_epoch++;
    _task_epoch_time[_driver_task_epoch % TASK_EPOCH_BUFSIZ].assign_now();
    if ((_driver_task_epoch % TASK_EPOCH_BUFSIZ) == 0)
	_task_epoch_first = _driver_task_epoch;
#endif
#if HAVE_ADAPTIVE_SCHEDULER
    Timestamp t_before = Timestamp::now();
#endif
#if CLICK_BSDMODULE && !BSD_NETISRSCHED
    int bsd_spl = splimp();
#endif

    // never run more than 32768 tasks
    if (ntasks > 32768)
	ntasks = 32768;

#if HAVE_MULTITHREAD
    // cycle counter for adaptive scheduling among processors
    click_cycles_t cycles = 0;
#endif

    Task::Status want_status;
    want_status.home_thread_id = thread_id();
    want_status.is_scheduled = true;
    want_status.is_strong_unscheduled = false;

    Task *t;
#if HAVE_MULTITHREAD
    int runs;
#endif
    bool work_done;

    for (; ntasks >= 0; --ntasks) {
	t = task_begin();
	if (t == task_end())
	    break;

	if (unlikely(t->_status.status != want_status.status)) {
	    t->remove_from_scheduled_list();
	    if (t->_status.home_thread_id != thread_id())
		t->move_thread_second_half();
	    continue;
	}

#if HAVE_MULTITHREAD
	runs = t->cycle_runs();
	if (runs > PROFILE_ELEMENT)
	    cycles = click_get_cycles();
#endif

	t->_status.is_scheduled = false;
	work_done = t->fire();

#if HAVE_MULTITHREAD
	if (runs > PROFILE_ELEMENT) {
	    unsigned delta = click_get_cycles() - cycles;
	    t->update_cycles(delta/32 + (t->cycles()*31)/32);
	}
#endif

	// fix task list
	if (t->scheduled()) {
	    // adjust position in scheduled list
#if HAVE_STRIDE_SCHED
	    t->_pass += t->_stride;
#endif

	    // If the task didn't do any work, don't run it next.  This might
	    // require delaying its pass, or exiting the scheduling loop
	    // entirely.
	    if (!work_done) {
#if HAVE_STRIDE_SCHED && HAVE_TASK_HEAP
		if (_task_heap.size() < 2)
		    break;
#else
		if (t->_next == &_task_link)
		    break;
#endif
#if HAVE_STRIDE_SCHED
# if HAVE_TASK_HEAP
		unsigned p1 = _task_heap.unchecked_at(1).pass;
		if (_task_heap.size() > 2 && PASS_GT(p1, _task_heap.unchecked_at(2).pass))
		    p1 = _task_heap.unchecked_at(2).pass;
# else
		unsigned p1 = t->_next->_pass;
# endif
		if (PASS_GT(p1, t->_pass))
		    t->_pass = p1;
#endif
	    }

#if HAVE_STRIDE_SCHED && HAVE_TASK_HEAP
	    task_reheapify_from(0, t);
#else
# if HAVE_STRIDE_SCHED
	    TaskLink *n = t->_next;
	    while (n != &_task_link && !PASS_GT(n->_pass, t->_pass))
		n = n->_next;
# else
	    TaskLink *n = &_task_link;
# endif
	    if (t->_next != n) {
		t->_next->_prev = t->_prev;
		t->_prev->_next = t->_next;
		t->_next = n;
		t->_prev = n->_prev;
		n->_prev->_next = t;
		n->_prev = t;
	    }
#endif
	} else
	    t->remove_from_scheduled_list();
    }

#if CLICK_BSDMODULE && !BSD_NETISRSCHED
    splx(bsd_spl);
#endif
#if HAVE_ADAPTIVE_SCHEDULER
    client_update_pass(C_CLICK, t_before);
#endif
}

inline void
RouterThread::run_os()
{
#if CLICK_LINUXMODULE
    // set state to interruptible early to avoid race conditions
    set_current_state(TASK_INTERRUPTIBLE);
#endif
    driver_unlock_tasks();
#if HAVE_ADAPTIVE_SCHEDULER
    Timestamp t_before = Timestamp::now();
#endif

#if CLICK_USERLEVEL
    select_set().run_selects(this);
#elif CLICK_MINIOS
    /*
     * MiniOS uses a cooperative scheduler. By schedule() we'll give a chance
     * to the OS threads to run.
     */
    schedule();
#elif CLICK_LINUXMODULE		/* Linux kernel module */
    if (_greedy) {
	if (time_after(jiffies, greedy_schedule_jiffies + 5 * CLICK_HZ)) {
	    greedy_schedule_jiffies = jiffies;
	    goto short_pause;
	}
    } else if (active()) {
      short_pause:
	set_thread_state(S_PAUSED);
	set_current_state(TASK_RUNNING);
	schedule();
    } else if (_id != 0) {
      block:
	set_thread_state(S_BLOCKED);
	schedule();
    } else if (Timestamp wait = timer_set().timer_expiry_steady_adjusted()) {
	wait -= Timestamp::now_steady();
	if (!(wait > Timestamp(0, Timestamp::subsec_per_sec / CLICK_HZ)))
	    goto short_pause;
	set_thread_state(S_TIMERWAIT);
	if (wait.sec() >= LONG_MAX / CLICK_HZ - 1)
	    (void) schedule_timeout(LONG_MAX - CLICK_HZ - 1);
	else
	    (void) schedule_timeout(wait.jiffies() - 1);
    } else
	goto block;
#elif defined(CLICK_BSDMODULE)
    if (_greedy)
	/* do nothing */;
    else if (active()) {	// just schedule others for a moment
	set_thread_state(S_PAUSED);
	yield(curthread, NULL);
    } else {
	set_thread_state(S_BLOCKED);
	_sleep_ident = &_sleep_ident;	// arbitrary address, != NULL
	tsleep(&_sleep_ident, PPAUSE, "pause", 1);
	_sleep_ident = NULL;
    }
#else
# error "Compiling for unknown target."
#endif

#if HAVE_ADAPTIVE_SCHEDULER
    client_update_pass(C_KERNEL, t_before);
#endif
    driver_lock_tasks();
}

void
RouterThread::process_pending()
{
    // must be called with thread's lock acquired

    // claim the current pending list
    set_thread_state(RouterThread::S_RUNPENDING);
    SpinlockIRQ::flags_t flags = _pending_lock.acquire();
    Task::Pending my_pending = _pending_head;
    _pending_head.x = 0;
    _pending_tail = &_pending_head;
    _pending_lock.release(flags);

    // process the list
    while (my_pending.x > 1) {
	Task *t = my_pending.t;
	my_pending = t->_pending_nextptr;
	t->_pending_nextptr.x = 0;
	click_fence();
	t->process_pending(this);
    }
}

void
RouterThread::driver()
{
    int iter = 0;
#if CLICK_LINUXMODULE
    // this task is running the driver
    _linux_task = current;
#elif CLICK_USERLEVEL
    select_set().initialize();
# if CLICK_USERLEVEL && HAVE_MULTITHREAD
    _running_processor = click_current_processor();
#  if HAVE___THREAD_STORAGE_CLASS
    click_current_thread_id = _id;
#  endif
# endif
#endif

#if CLICK_NS
    {
	Timestamp now = Timestamp::now();
	if (now >= _ns_scheduled)
	    _ns_scheduled.set_sec(-1);
    }
#endif

    driver_lock_tasks();

#if HAVE_ADAPTIVE_SCHEDULER
    client_set_tickets(C_CLICK, DRIVER_TOTAL_TICKETS / 2);
    client_set_tickets(C_KERNEL, DRIVER_TOTAL_TICKETS / 2);
    _cur_click_share = Task::MAX_UTILIZATION / 2;
    _adaptive_restride_timestamp.assign_now();
    _adaptive_restride_iter = 0;
#endif

    while (1) {
#if CLICK_DEBUG_SCHEDULING
	_driver_epoch++;
#endif

#if !BSD_NETISRSCHED
	// check to see if driver is stopped
	if (_stop_flag > 0) {
	    driver_unlock_tasks();
	    bool b = _master->check_driver();
	    driver_lock_tasks();
	    if (!b)
		break;
	}
#endif

	// run occasional tasks: timers, select, etc.
	iter++;

	// run task requests
	click_compiler_fence();
	if (_pending_head.x)
	    process_pending();

	// run tasks
	do {
#if HAVE_ADAPTIVE_SCHEDULER
	    if (PASS_GT(_clients[C_CLICK].pass, _clients[C_KERNEL].pass))
		break;
#endif
	    run_tasks(_tasks_per_iter);
	} while (0);

#if CLICK_USERLEVEL
	// run signals
	run_signals();
#endif

	// run timers
	do {
#if !BSD_NETISRSCHED
	    if (iter % timer_set().timer_stride())
		break;
#elif BSD_NETISRSCHED
	    if (iter % timer_set().timer_stride() && _oticks == ticks)
		break;
	    _oticks = ticks;
#endif
	    timer_set().run_timers(this, _master);
	} while (0);

	// run operating system
	do {
#if !HAVE_ADAPTIVE_SCHEDULER && !BSD_NETISRSCHED
	    if (iter % _iters_per_os)
		break;
#elif HAVE_ADAPTIVE_SCHEDULER
	    if (!PASS_GT(_clients[C_CLICK].pass, _clients[C_KERNEL].pass))
		break;
#elif BSD_NETISRSCHED
	    break;
#endif
	    run_os();
	} while (0);

#if CLICK_NS || BSD_NETISRSCHED
	// Everyone except the NS driver stays in driver() until the driver is
	// stopped.
	break;
#endif
    }

    driver_unlock_tasks();

#if HAVE_ADAPTIVE_SCHEDULER
    _cur_click_share = 0;
#endif
#if CLICK_LINUXMODULE
    _linux_task = 0;
#endif
#if CLICK_USERLEVEL && HAVE_MULTITHREAD
    _running_processor = click_invalid_processor();
# if HAVE___THREAD_STORAGE_CLASS
    click_current_thread_id = 0;
# endif
#endif
#if CLICK_NS
    do {
	Timestamp t = Timestamp::uninitialized_t();
	if (active()) {
	    t = Timestamp::now();
	    if (t != _ns_last_active) {
		_ns_active_iter = 0;
		_ns_last_active = t;
	    } else if (++_ns_active_iter >= ns_iters_per_time)
		t += Timestamp::epsilon();
	} else if (Timestamp next_expiry = timer_set().timer_expiry_steady())
	    t = next_expiry;
	else
	    break;
	if (t >= _ns_scheduled && _ns_scheduled.sec() >= 0)
	    break;
	if (Timestamp::schedule_granularity == Timestamp::usec_per_sec) {
	    t = t.usec_ceil();
	    struct timeval tv = t.timeval();
	    simclick_sim_command(_master->simnode(), SIMCLICK_SCHEDULE, &tv);
	} else {
	    t = t.nsec_ceil();
	    struct timespec ts = t.timespec();
	    simclick_sim_command(_master->simnode(), SIMCLICK_SCHEDULE, &ts);
	}
	_ns_scheduled = t;
    } while (0);
#endif
}


void
RouterThread::kill_router(Router *r)
{
    lock_tasks();
#if HAVE_TASK_HEAP
    Task *t;
    for (task_heap_element *tp = _task_heap.end(); tp > _task_heap.begin(); )
	if ((t = (--tp)->t, t->router() == r)) {
	    task_reheapify_from(tp - _task_heap.begin(), _task_heap.back().t);
	    // must clear _schedpos AFTER task_reheapify_from
	    t->_schedpos = -1;
	    // recheck this slot; have moved a task there
	    _task_heap.pop_back();
	    if (tp < _task_heap.end())
		tp++;
	}
#else
    TaskLink *prev = &_task_link;
    TaskLink *t;
    for (t = prev->_next; t != &_task_link; t = t->_next)
	if (static_cast<Task *>(t)->router() == r)
	    t->_prev = 0;
	else {
	    prev->_next = t;
	    t->_prev = prev;
	    prev = t;
	}
    prev->_next = t;
    t->_prev = prev;
#endif
    unlock_tasks();

    _timers.kill_router(r);
#if CLICK_USERLEVEL
    _selects.kill_router(r);
#endif
}

#if CLICK_DEBUG_SCHEDULING
String
RouterThread::thread_state_name(int ts)
{
    switch (ts) {
    case S_PAUSED:		return String::make_stable("paused");
    case S_BLOCKED:		return String::make_stable("blocked");
    case S_TIMERWAIT:		return String::make_stable("timerwait");
    case S_LOCKSELECT:		return String::make_stable("lockselect");
    case S_LOCKTASKS:		return String::make_stable("locktasks");
    case S_RUNTASK:		return String::make_stable("runtask");
    case S_RUNTIMER:		return String::make_stable("runtimer");
    case S_RUNSIGNAL:		return String::make_stable("runsignal");
    case S_RUNPENDING:		return String::make_stable("runpending");
    case S_RUNSELECT:		return String::make_stable("runselect");
    default:			return String(ts);
    }
}
#endif

CLICK_ENDDECLS
