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
#ifdef CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/sched.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
CLICK_DECLS

#define DEBUG_RT_SCHED		0

#ifdef HAVE_ADAPTIVE_SCHEDULER
# define DRIVER_TASKS_PER_ITER	128
#elif defined(CLICK_NS)
# define DRIVER_TASKS_PER_ITER	256
#else
# define DRIVER_TASKS_PER_ITER	128
#endif
#define PROFILE_ELEMENT		20

#ifdef CLICK_NS
# define DRIVER_ITER_ANY	1
# define DRIVER_ITER_TIMERS	1
#else
# define DRIVER_ITER_ANY	32
# define DRIVER_ITER_TIMERS	32
#endif
#ifdef CLICK_USERLEVEL
# define DRIVER_ITER_OS		64	/* iterations per select() */
#else
# define DRIVER_ITER_OS		256	/* iterations per OS schedule() */
#endif

#ifdef HAVE_ADAPTIVE_SCHEDULER
# define DRIVER_TOTAL_TICKETS	128	/* # tickets shared between clients */
# define DRIVER_GLOBAL_STRIDE	(Task::STRIDE1 / DRIVER_TOTAL_TICKETS)
# define DRIVER_QUANTUM		8	/* microseconds per stride */
# define DRIVER_RESTRIDE_INTERVAL 80	/* microseconds between restrides */
#endif


RouterThread::RouterThread(Router *r)
    : Task(Task::error_hook, 0), _router(r)
{
    _prev = _next = _thread = this;
#ifdef CLICK_BSDMODULE
    _wakeup_list = 0;
#endif
#ifdef HAVE_ADAPTIVE_SCHEDULER
    _max_click_share = 80 * Task::MAX_UTILIZATION / 100;
    _min_click_share = Task::MAX_UTILIZATION / 200;
    _cur_click_share = 0;	// because we aren't yet running
#endif
    _task_lock_waiting = 0;
    _pending = 0;
    router()->add_thread(this);
    // add_thread() will call this->set_thread_id()
}

RouterThread::~RouterThread()
{
    unschedule_all_tasks();
    router()->remove_thread(this);
}

inline void
RouterThread::process_task_requests()
{
    TaskList *task_list = router()->task_list();
    task_list->lock();
    Task *t = task_list->_pending_next;
    task_list->_pending_next = task_list;
    _pending = 0;
    task_list->unlock();

    while (t != task_list) {
	Task *next = t->_pending_next;
	t->_pending_next = 0;
	t->process_pending(this);
	t = next;
    }
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
/* The driver loop            */
/******************************/

/* Run at most 'ntasks' tasks. */
inline void
RouterThread::run_tasks(int ntasks)
{
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

inline void
RouterThread::run_os()
{
    unlock_tasks();

#if CLICK_USERLEVEL
    router()->run_selects(!empty());
#elif !defined(CLICK_GREEDY)
# if CLICK_LINUXMODULE			/* Linux kernel module */
    schedule();
# else					/* BSD kernel module */
    extern int click_thread_priority;
    int s = splhigh();
    curproc->p_priority = curproc->p_usrpri = click_thread_priority;
    setrunqueue(curproc);
    mi_switch();
    splx(s);
# endif
#endif
    
    nice_lock_tasks();
}

void
RouterThread::driver()
{
    const volatile int * const runcount = _router->driver_runcount_ptr();
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

#ifndef CLICK_NS
  driver_loop:
#endif

#ifndef HAVE_ADAPTIVE_SCHEDULER
    // run a bunch of tasks
    run_tasks(DRIVER_TASKS_PER_ITER);
#else
    click_gettimeofday(&t_before);
    int client;
    if (PASS_GT(_clients[C_KERNEL].pass, _clients[C_CLICK].pass)) {
	client = C_CLICK;
	run_tasks(DRIVER_TASKS_PER_ITER);
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

    if (*runcount > 0) {
	// run task requests
	if (_pending)
	    process_task_requests();

	// run occasional tasks: timers, select, etc.
	iter++;
	if (iter % DRIVER_ITER_ANY == 0)
	    wait(iter);
    }

#ifndef CLICK_NS
    // run loop again, unless driver is stopped
    if (*runcount > 0 || _router->check_driver())
	goto driver_loop;
#endif
    
    unlock_tasks();

#ifdef HAVE_ADAPTIVE_SCHEDULER
    _cur_click_share = 0;
#endif
}

void
RouterThread::wait(int iter)
{
#ifndef HAVE_ADAPTIVE_SCHEDULER	/* Adaptive scheduler runs OS itself. */
    if (thread_id() == 0 && (iter % DRIVER_ITER_OS) == 0)
	run_os();
#endif

    if (iter % DRIVER_ITER_TIMERS == 0) {
	router()->run_timers();
#ifdef CLICK_NS
	// If there's another timer, tell the simulator to make us
	// run when it's due to go off.
	struct timeval now, nextdelay, nexttime;
	if (router()->timer_list()->get_next_delay(&nextdelay)) {
	    click_gettimeofday(&now);
	    timeradd(&now, &nextdelay, &nexttime);
	    simclick_sim_schedule(router()->get_siminst(),
				  router()->get_clickinst(), &nexttime);
	}
#endif
    }
}


/******************************/
/* Secondary driver functions */
/******************************/

void
RouterThread::driver_once()
{
    if (!_router->check_driver())
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

CLICK_ENDDECLS
