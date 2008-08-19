// -*- c-basic-offset: 4; related-file-name: "../include/click/routerthread.hh" -*-
/*
 * routerthread.{cc,hh} -- Click threads
 * Eddie Kohler, Benjie Chen, Petros Zerfos
 *
 * Copyright (c) 2000-2001 Massachusetts Institute of Technology
 * Copyright (c) 2001-2002 International Computer Science Institute
 * Copyright (c) 2004-2007 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
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
#endif
CLICK_DECLS

#define DEBUG_RT_SCHED		0

#define PROFILE_ELEMENT		20

#ifdef HAVE_ADAPTIVE_SCHEDULER
# define DRIVER_TOTAL_TICKETS	128	/* # tickets shared between clients */
# define DRIVER_GLOBAL_STRIDE	(Task::STRIDE1 / DRIVER_TOTAL_TICKETS)
# define DRIVER_QUANTUM		8	/* microseconds per stride */
# define DRIVER_RESTRIDE_INTERVAL 80	/* microseconds between restrides */
#endif

#if CLICK_DEBUG_SCHEDULING
# define SET_STATE(s)		_thread_state = (s)
#else
# define SET_STATE(s)		/* nada */
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

RouterThread::RouterThread(Master *m, int id)
#if HAVE_TASK_HEAP
    : _task_heap_hole(0), _master(m), _id(id)
#else
    : Task(Task::error_hook, 0), _master(m), _id(id)
#endif
{
#if HAVE_TASK_HEAP
    _pass = 0;
#else
    _prev = _next = _thread = this;
#endif
    _any_pending = 0;
#if CLICK_LINUXMODULE
    _linux_task = 0;
    _task_lock_waiting = 0;
    spin_lock_init(&_lock);
#elif HAVE_MULTITHREAD
    _running_processor = click_invalid_processor();
    _task_lock_waiting = 0;
#endif
#ifdef HAVE_ADAPTIVE_SCHEDULER
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

#if CLICK_USERLEVEL
    _iters_per_os = 64;           /* iterations per select() */
#else
    _iters_per_os = 2;          /* iterations per OS schedule() */
#endif

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
    _greedy = false;
#endif
#if CLICK_LINUXMODULE
    greedy_schedule_jiffies = jiffies;
#endif
    
#if CLICK_DEBUG_SCHEDULING
    _thread_state = S_BLOCKED;
    _driver_epoch = 0;
    _driver_task_epoch = 0;
    _task_epoch_first = 0;
#endif

    static_assert(THREAD_QUIESCENT == (int) ThreadSched::THREAD_QUIESCENT
		  && THREAD_STRONG_UNSCHEDULE == (int) ThreadSched::THREAD_STRONG_UNSCHEDULE
		  && THREAD_UNKNOWN == (int) ThreadSched::THREAD_UNKNOWN);
}

RouterThread::~RouterThread()
{
    _any_pending = 0;
    assert(!active());
}

inline void
RouterThread::driver_lock_tasks()
{
    // If other people are waiting for the task lock, give them a chance to
    // catch it before we claim it.
#if CLICK_LINUXMODULE
    for (int i = 0; _task_lock_waiting > 0 && i < 10; i++)
	schedule();
    spin_lock(&_lock);
#elif HAVE_MULTITHREAD
# if CLICK_USERLEVEL
    for (int i = 0; _task_lock_waiting > 0 && i < 10; i++) {
	struct timeval waiter = { 0, 1 };
	select(0, 0, 0, 0, &waiter);
    }
# endif
    _lock.acquire();
#endif
}

inline void
RouterThread::driver_unlock_tasks()
{
#if CLICK_LINUXMODULE
    spin_unlock(&_lock);
#elif HAVE_MULTITHREAD
    _lock.release();
#endif
}


/******************************/
/* Adaptive scheduler         */
/******************************/

#ifdef HAVE_ADAPTIVE_SCHEDULER

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
RouterThread::client_update_pass(int client, const struct timeval &t_before, const struct timeval &t_after)
{
    Client &c = _clients[client];
    int elapsed = (1000000 * (t_after.tv_sec - t_before.tv_sec)) + (t_after.tv_usec - t_before.tv_usec);
    if (elapsed > 0)
	c.pass += (c.stride * elapsed) / DRIVER_QUANTUM;
    else
	c.pass += c.stride;
}

inline void
RouterThread::check_restride(struct timeval &t_before, const struct timeval &t_now, int &restride_iter)
{
    int elapsed = (1000000 * (t_now.tv_sec - t_before.tv_sec)) + (t_now.tv_usec - t_before.tv_usec);
    if (elapsed > DRIVER_RESTRIDE_INTERVAL || elapsed < 0) {
	// mark new measurement period
	t_before = t_now;
	
	// reset passes every 10 intervals, or when time moves backwards
	if (++restride_iter == 10 || elapsed < 0) {
	    _global_pass = _clients[C_CLICK].tickets = _clients[C_KERNEL].tickets = 0;
	    restride_iter = 0;
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
timeval
RouterThread::task_epoch_time(uint32_t epoch) const
{
    if (epoch >= _task_epoch_first && epoch <= _driver_task_epoch)
	return _task_epoch_time[epoch - _task_epoch_first];
    else if (epoch > _driver_task_epoch - TASK_EPOCH_BUFSIZ && epoch <= _task_epoch_first - 1)
	// "-1" makes this code work even if _task_epoch overflows
	return _task_epoch_time[epoch - (_task_epoch_first - TASK_EPOCH_BUFSIZ)];
    else
	return make_timeval(0, 0);
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
    Task** tbegin = _task_heap.begin();
    Task** tend = _task_heap.end();
    int npos;
    
    while (pos > 0
	   && (npos = (pos-1) >> 1, PASS_GT(tbegin[npos]->_pass, t->_pass))) {
	tbegin[pos] = tbegin[npos];
	tbegin[npos]->_schedpos = pos;
	pos = npos;
    }

    while (1) {
	Task* smallest = t;
	Task** tp = tbegin + 2*pos + 1;
	if (tp < tend && PASS_GE(smallest->_pass, tp[0]->_pass))
	    smallest = tp[0];
	if (tp + 1 < tend && PASS_GE(smallest->_pass, tp[1]->_pass))
	    smallest = tp[1], tp++;

	smallest->_schedpos = pos;
	tbegin[pos] = smallest;

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
#if CLICK_DEBUG_SCHEDULING
    _driver_task_epoch++;
    click_gettimeofday(&_task_epoch_time[_driver_task_epoch % TASK_EPOCH_BUFSIZ]);
    if ((_driver_task_epoch % TASK_EPOCH_BUFSIZ) == 0)
	_task_epoch_first = _driver_task_epoch;
#endif
    
    // never run more than 32768 tasks
    if (ntasks > 32768)
	ntasks = 32768;

#if HAVE_MULTITHREAD
    // cycle counter for adaptive scheduling among processors
    click_cycles_t cycles = 0;
#endif

    Task *t;
#if HAVE_TASK_HEAP
    while (_task_heap.size() > 0 && ntasks >= 0) {
	t = _task_heap.at_u(0);
#else
    while ((t = task_begin()), t != this && ntasks >= 0) {
#endif

	// 22.May.2008: If pending changes on this task, break early to
	// take care of them.
	if (t->_pending_nextptr)
	    break;

#if HAVE_MULTITHREAD
	int runs = t->cycle_runs();
	if (runs > PROFILE_ELEMENT)
	    cycles = click_get_cycles();
#endif

#if HAVE_TASK_HEAP
	t->_schedpos = -1;
	_task_heap_hole = 1;
#else
	t->fast_unschedule(false);
#endif

#if HAVE_STRIDE_SCHED
	// 21.May.2007: Always set the current thread's pass to the current
	// task's pass, to avoid problems when fast_reschedule() interacts
	// with fast_schedule() (passes got out of sync).
	_pass = t->_pass;
#endif
	
	t->fire();

#if HAVE_TASK_HEAP
	if (_task_heap_hole) {
	    Task* back = _task_heap.back();
	    _task_heap.pop_back();
	    if (_task_heap.size() > 0)
		task_reheapify_from(0, back);
	    _task_heap_hole = 0;
	    // No need to reset t->_schedpos: 'back == t' only if
	    // '_task_heap.size() == 0' now, in which case we didn't call
	    // task_reheapify_from().
	}
#endif

#if HAVE_MULTITHREAD
	if (runs > PROFILE_ELEMENT) {
	    unsigned delta = click_get_cycles() - cycles;
	    t->update_cycles(delta/32 + (t->cycles()*31)/32);
	}
#endif

	--ntasks;
    }
}

inline void
RouterThread::run_os()
{
#if CLICK_LINUXMODULE
    // set state to interruptible early to avoid race conditions
    set_current_state(TASK_INTERRUPTIBLE);
#endif
    driver_unlock_tasks();

#if CLICK_USERLEVEL
    _master->run_selects(active());
#elif CLICK_LINUXMODULE		/* Linux kernel module */
    if (_greedy) {
	if (time_after(jiffies, greedy_schedule_jiffies + 5 * CLICK_HZ)) {
	    greedy_schedule_jiffies = jiffies;
	    goto short_pause;
	}
    } else if (active()) {
      short_pause:
	SET_STATE(S_PAUSED);
	set_current_state(TASK_RUNNING);
	schedule();
    } else if (_id != 0) {
      block:
	SET_STATE(S_BLOCKED);
	schedule();
    } else if (Timestamp wait = _master->next_timer_expiry_adjusted()) {
	wait -= Timestamp::now();
	if (!(wait.sec() > 0 || (wait.sec() == 0 && wait.subsec() > (Timestamp::NSUBSEC / CLICK_HZ))))
	    goto short_pause;
	SET_STATE(S_TIMER);
	if (wait.sec() >= LONG_MAX / CLICK_HZ - 1)
	    (void) schedule_timeout(LONG_MAX - CLICK_HZ - 1);
	else
	    (void) schedule_timeout((wait.sec() * CLICK_HZ) + (wait.subsec() * CLICK_HZ / Timestamp::NSUBSEC) - 1);
    } else
	goto block;
    SET_STATE(S_RUNNING);
#elif defined(CLICK_BSDMODULE)
    if (_greedy)
	/* do nothing */;
    else if (active()) {	// just schedule others for a moment
	yield(curthread, NULL);
    } else {
	_sleep_ident = &_sleep_ident;	// arbitrary address, != NULL
	tsleep(&_sleep_ident, PPAUSE, "pause", 1);
	_sleep_ident = NULL;
    }
#else
# error "Compiling for unknown target."
#endif
    
    driver_lock_tasks();
}

void
RouterThread::driver()
{
    const volatile int * const stopper = _master->stopper_ptr();
    int iter = 0;
#if CLICK_LINUXMODULE
    // this task is running the driver
    _linux_task = current;
#elif HAVE_MULTITHREAD
    _running_processor = click_current_processor();
#endif

    driver_lock_tasks();
    
#ifdef HAVE_ADAPTIVE_SCHEDULER
    int restride_iter = 0;
    struct timeval t_before, restride_t_before, t_now;
    client_set_tickets(C_CLICK, DRIVER_TOTAL_TICKETS / 2);
    client_set_tickets(C_KERNEL, DRIVER_TOTAL_TICKETS / 2);
    _cur_click_share = Task::MAX_UTILIZATION / 2;
    click_gettimeofday(&restride_t_before);
#endif

    SET_STATE(S_RUNNING);

#if !CLICK_NS
  driver_loop:
#endif

#if CLICK_DEBUG_SCHEDULING
    _driver_epoch++;
#endif

    if (*stopper == 0) {
	// run occasional tasks: timers, select, etc.
	iter++;

#if CLICK_USERLEVEL
	_master->run_signals();
#endif
	
#if !(HAVE_ADAPTIVE_SCHEDULER||BSD_NETISRSCHED)
	if ((iter % _iters_per_os) == 0)
	    run_os();
#endif

#ifdef BSD_NETISRSCHED
	if ((iter % _master->timer_stride()) == 0 || _oticks != ticks) {
	    _oticks = ticks;
#else
	if ((iter % _master->timer_stride()) == 0) {
#endif
	    _master->run_timers();
#if CLICK_NS
	    // If there's another timer, tell the simulator to make us
	    // run when it's due to go off.
	    if (Timestamp next_expiry = _master->next_timer_expiry()) {
		struct timeval nexttime = next_expiry.timeval();
		simclick_sim_command(_master->simnode(), SIMCLICK_SCHEDULE, &nexttime);
	    }
#endif
	}
    }

    // run task requests (1)
    if (_any_pending)
	_master->process_pending(this);

#ifndef HAVE_ADAPTIVE_SCHEDULER
    // run a bunch of tasks
# if CLICK_BSDMODULE && !BSD_NETISRSCHED
    int s = splimp();
# endif
    run_tasks(_tasks_per_iter);
# if CLICK_BSDMODULE && !BSD_NETISRSCHED
    splx(s);
# endif
#else /* HAVE_ADAPTIVE_SCHEDULER */
    click_gettimeofday(&t_before);
    int client;
    if (PASS_GT(_clients[C_KERNEL].pass, _clients[C_CLICK].pass)) {
	client = C_CLICK;
	run_tasks(_tasks_per_iter);
    } else {
	client = C_KERNEL;
	run_os();
    }
    click_gettimeofday(&t_now);
    client_update_pass(client, t_before, t_now);
    check_restride(restride_t_before, t_now, restride_iter);
#endif

#ifndef BSD_NETISRSCHED
    // check to see if driver is stopped
    if (*stopper > 0) {
	driver_unlock_tasks();
	bool b = _master->check_driver();
	driver_lock_tasks();
	if (!b)
	    goto finish_driver;
    }
#endif
    
#if !CLICK_NS && !BSD_NETISRSCHED
    // Everyone except the NS driver stays in driver() until the driver is
    // stopped.
    goto driver_loop;
#endif

  finish_driver:
    driver_unlock_tasks();

#ifdef HAVE_ADAPTIVE_SCHEDULER
    _cur_click_share = 0;
#endif
#if CLICK_LINUXMODULE
    _linux_task = 0;
#elif HAVE_MULTITHREAD
    _running_processor = click_invalid_processor();
#endif
}


/******************************/
/* Secondary driver functions */
/******************************/

void
RouterThread::driver_once()
{
    if (!_master->check_driver())
	return;
  
#ifdef CLICK_BSDMODULE  /* XXX MARKO */
    int s = splimp();
#endif
#if CLICK_LINUXMODULE
    // this task is running the driver
    _linux_task = current;
    spin_lock(&_lock);
#elif HAVE_MULTITHREAD
    _running_processor = click_current_processor();
    _lock.acquire();
#endif
    Task *t = task_begin();
    if (t != task_end() && !t->_pending_nextptr) {
	t->fast_unschedule(false);
	t->fire();
    }
#ifdef CLICK_BSDMODULE  /* XXX MARKO */
    splx(s);
#endif
#if CLICK_LINUXMODULE
    spin_unlock(&_lock);
    _linux_task = 0;
#elif HAVE_MULTITHREAD
    _lock.release();
    _running_processor = click_invalid_processor();
#endif
}

void
RouterThread::unschedule_router_tasks(Router* r)
{
    lock_tasks();
#if HAVE_TASK_HEAP
    Task* t;
    for (Task** tp = _task_heap.end(); tp > _task_heap.begin(); )
	if ((t = *--tp, t->_router == r)) {
	    task_reheapify_from(tp - _task_heap.begin(), _task_heap.back());
	    // must clear _schedpos AFTER task_reheapify_from
	    t->_schedpos = -1;
	    // recheck this slot; have moved a task there
	    _task_heap.pop_back();
	    if (tp < _task_heap.end())
		tp++;
	}
#else
    Task* prev = this;
    Task* t;
    for (t = prev->_next; t != this; t = t->_next)
	if (t->_router == r)
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
}

#if CLICK_DEBUG_SCHEDULING
String
RouterThread::thread_state_name(int ts)
{
    switch (ts) {
      case S_RUNNING:		return "running";
      case S_PAUSED:		return "paused";
      case S_TIMER:		return "timer";
      case S_BLOCKED:		return "blocked";
      default:			return String(ts);
    }
}
#endif

CLICK_ENDDECLS
