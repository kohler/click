// -*- c-basic-offset: 4; related-file-name: "../include/click/routerthread.hh" -*-
/*
 * routerthread.{cc,hh} -- Click threads
 * Benjie Chen, Eddie Kohler, Petros Zerfos
 *
 * Copyright (c) 2000-2001 Massachusetts Institute of Technology
 * Copyright (c) 2001-2002 International Computer Science Institute
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
#ifdef CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/sched.h>
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


RouterThread::RouterThread(Master *m, int id)
    : Task(Task::error_hook, 0), _master(m), _id(id)
{
    _prev = _next = _thread = this;
    _task_lock_waiting = 0;
    _pending = 0;
#ifdef CLICK_LINUXMODULE
    _sleeper = 0;
#endif
#ifdef CLICK_BSDMODULE
    _wakeup_list = 0;
#endif
#ifdef HAVE_ADAPTIVE_SCHEDULER
    _max_click_share = 80 * Task::MAX_UTILIZATION / 100;
    _min_click_share = Task::MAX_UTILIZATION / 200;
    _cur_click_share = 0;	// because we aren't yet running
#endif

#if defined(CLICK_NS)
    _tasks_per_iter = 256;
    _iters_per_timers = 1;
#else
    _tasks_per_iter = 128;
    _iters_per_timers = 32;
#endif

#ifdef CLICK_USERLEVEL
    _iters_per_os = 64;           /* iterations per select() */
#else
    _iters_per_os = 2;          /* iterations per OS schedule() */
#endif

#if CLICK_DEBUG_SCHEDULING
    _thread_state = S_BLOCKED;
    _driver_epoch = 0;
    _driver_task_epoch = 0;
    _task_epoch_first = 0;
#endif
}

RouterThread::~RouterThread()
{
    unschedule_all_tasks();
}

inline void
RouterThread::nice_lock_tasks()
{
#if CLICK_LINUXMODULE
    // If other people are waiting for the task lock, give them a second to
    // catch it before we claim it.
    if (_task_lock_waiting > 0) {
	unsigned long done_jiffies = click_jiffies() + CLICK_HZ;
	while (_task_lock_waiting > 0 && click_jiffies() < done_jiffies)
	    /* XXX schedule() instead of spinlock? */;
    }
#endif
    lock_tasks();
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
    if (min_frac > MAX_UTILIZATION - 1)
	min_frac = MAX_UTILIZATION - 1;
    if (max_frac > MAX_UTILIZATION - 1)
	max_frac = MAX_UTILIZATION - 1;
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
	for (Task *t = scheduled_next(); t != this; t = t->scheduled_next()) {
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

#if __MTCLICK__
    // cycle counter for adaptive scheduling among processors
    uint64_t cycles;
#endif

    Task *t;
    while ((t = scheduled_next()), t != this && ntasks >= 0) {

#if __MTCLICK__
	int runs = t->fast_unschedule();
	if (runs > PROFILE_ELEMENT)
	    cycles = click_get_cycles();
#else
	t->fast_unschedule();
#endif

	t->call_hook();

#if __MTCLICK__
	if (runs > PROFILE_ELEMENT) {
	    unsigned delta = click_get_cycles() - cycles;
	    t->update_cycles(delta/32 + (t->cycles()*31)/32);
	}
#endif

	ntasks--;
    }
}

#if CLICK_DEBUG_SCHEDULING
# define SET_STATE(s)		_thread_state = (s)
#else
# define SET_STATE(s)		/* nada */
#endif

inline void
RouterThread::run_os()
{
#if CLICK_LINUXMODULE
    // set state to interruptible early to avoid race conditions
    _sleeper = current;
    current->state = TASK_INTERRUPTIBLE;
#endif
    unlock_tasks();

#if CLICK_USERLEVEL
    _master->run_selects(!empty());
#elif !defined(CLICK_GREEDY)
# if CLICK_LINUXMODULE		/* Linux kernel module */
    if (!empty()) {		// just schedule others for a second
	current->state = TASK_RUNNING;
	SET_STATE(S_PAUSED);
	schedule();
    } else {
	struct timeval wait;
	if (_id != 0 || !_master->timer_delay(&wait)) {
	    SET_STATE(S_BLOCKED);
	    schedule();
	} else if (wait.tv_sec > 0 || wait.tv_usec > (1000000 / CLICK_HZ)) {
	    SET_STATE(S_TIMER);
	    (void) schedule_timeout((wait.tv_sec * CLICK_HZ) + (wait.tv_usec * CLICK_HZ / 1000000) - 1);
	} else {
	    current->state = TASK_RUNNING;
	    SET_STATE(S_PAUSED);
	    schedule();
	}
    }
    SET_STATE(S_RUNNING);
# else				/* BSD kernel module */
    extern int click_thread_priority;
    int s = splhigh();
    curproc->p_priority = curproc->p_usrpri = click_thread_priority;
    setrunqueue(curproc);
    mi_switch();
    splx(s);
# endif
#endif
    
    nice_lock_tasks();

#if CLICK_LINUXMODULE
    // set state to interruptible early to avoid race conditions
    _sleeper = 0;
#endif
}

void
RouterThread::driver()
{
    const volatile int * const runcount = _master->runcount_ptr();
    int iter = 0;

    nice_lock_tasks();
    
#ifdef HAVE_ADAPTIVE_SCHEDULER
    int restride_iter = 0;
    struct timeval t_before, restride_t_before, t_now;
    client_set_tickets(C_CLICK, DRIVER_TOTAL_TICKETS / 2);
    client_set_tickets(C_KERNEL, DRIVER_TOTAL_TICKETS / 2);
    _cur_click_share = Task::MAX_UTILIZATION / 2;
    click_gettimeofday(&restride_t_before);
#endif

    SET_STATE(S_RUNNING);

#ifndef CLICK_NS
  driver_loop:
#endif

#if CLICK_DEBUG_SCHEDULING
    _driver_epoch++;
#endif

    if (*runcount > 0) {
	// run occasional tasks: timers, select, etc.
	iter++;
	
#ifndef HAVE_ADAPTIVE_SCHEDULER	/* Adaptive scheduler runs OS itself. */
	if ((iter % _iters_per_os) == 0)
	    run_os();
#endif

	if ((iter % _iters_per_timers) == 0) {
	    _master->run_timers();
#ifdef CLICK_NS
	    // If there's another timer, tell the simulator to make us
	    // run when it's due to go off.
	    struct timeval now, nextdelay, nexttime;
	    if (_master->timer_delay(&nextdelay)) {
		click_gettimeofday(&now);
		timeradd(&now, &nextdelay, &nexttime);
		simclick_sim_schedule(_master->_siminst, _master->_clickinst, &nexttime);
	    }
#endif
	}
    }

    // run task requests (1)
    if (_pending)
	_master->process_pending(this);

#ifndef HAVE_ADAPTIVE_SCHEDULER
    // run a bunch of tasks
    run_tasks(_tasks_per_iter);
#else
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

#ifdef CLICK_BSDMODULE
    // wake up tasks that went to sleep, waiting on packets
    if (_wakeup_list) {
	int s = splimp();
	Task *t;
	while ((t = _wakeup_list) != 0) {
	    _wakeup_list = t->_next;
	    t->reschedule();
	}
	splx(s);
    }
#endif

    // run loop again, unless driver is stopped
    if (*runcount > 0) {
#ifndef CLICK_NS
	// Everyone except the NS driver stays in driver() until the driver is
	// stopped.
	goto driver_loop;
#endif
    } else {
	unlock_tasks();
	bool b = _master->check_driver();
	nice_lock_tasks();
	if (b)
	    goto driver_loop;
    }

    unlock_tasks();

#ifdef HAVE_ADAPTIVE_SCHEDULER
    _cur_click_share = 0;
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
  
    lock_tasks();
    Task *t = scheduled_next();
    if (t != this) {
	t->fast_unschedule();
	t->call_hook();
    }
    unlock_tasks();
}

void
RouterThread::unschedule_all_tasks()
{
    lock_tasks();
    Task *t;
    while ((t = scheduled_next()), t != this)
	t->fast_unschedule();
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
