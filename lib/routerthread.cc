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

#ifdef CLICK_NS
#define DRIVER_TASKS_PER_ITER	256
#else
#define DRIVER_TASKS_PER_ITER	128
#endif
#define PROFILE_ELEMENT		20

#ifdef CLICK_NS
#define DRIVER_ITER_ANY		1
#define DRIVER_ITER_TIMERS	1
#else
#define DRIVER_ITER_ANY		32
#define DRIVER_ITER_TIMERS	32
#endif
#define DRIVER_ITER_SELECT	64
#define DRIVER_ITER_LINUXSCHED	256
#define DRIVER_ITER_BSDSCHED	256


RouterThread::RouterThread(Router *r)
    : Task(Task::error_hook, 0), _router(r)
{
    _prev = _next = _thread = this;
#ifdef CLICK_BSDMODULE
    _wakeup_list = 0;
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


/*******************/
/* The driver loop */
/*                 */
/*******************/

/* Run at most 'ntasks' tasks. */
inline void
RouterThread::run_tasks(int ntasks)
{
    // never run more than 32768 tasks
    if (ntasks > 32768)
	ntasks = 32768;

#if HAVE_ADAPTIVE_SCHEDULER
    // how much work has Click successfully accomplished?
    int work_done = 0;
#endif

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

#if HAVE_ADAPTIVE_SCHEDULER
	work_done += t->call_hook();
#else
	t->call_hook();
#endif

#if __MTCLICK__
	if (runs > PROFILE_ELEMENT) {
	    unsigned delta = click_get_cycles() - cycles;
	    t->update_cycles(cycles/32 + (t->cycles()*31)/32);
	}
#endif

	ntasks--;
    }

#if HAVE_ADAPTIVE_SCHEDULER
    _task_work_done += work_done;
#endif
}

void
RouterThread::driver()
{
    const volatile int * const runcount = _router->driver_runcount_ptr();
    int iter = 0;
  
    nice_lock_tasks();

# ifndef CLICK_NS
  driver_loop:
# endif
    // run a bunch of tasks
    run_tasks(DRIVER_TASKS_PER_ITER);

# ifdef CLICK_BSDMODULE
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
# endif

    if (*runcount > 0) {
	// run task requests
	if (_pending)
	    process_task_requests();

	// run occasional tasks: timers, select, etc.
	iter++;
	if (iter % DRIVER_ITER_ANY == 0)
	    wait(iter);
    }

# ifndef CLICK_NS
    // run loop again, unless driver is stopped
    if (*runcount > 0 || _router->check_driver())
	goto driver_loop;
# endif
    
    unlock_tasks();
}

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
RouterThread::wait(int iter)
{
    if (thread_id() == 0) {
	unlock_tasks();
#if CLICK_USERLEVEL
	if (iter % DRIVER_ITER_SELECT == 0)
	    router()->run_selects(!empty());
#else
# ifndef CLICK_GREEDY
#  if defined(CLICK_LINUXMODULE)	/* Linux kernel module */
	if (iter % DRIVER_ITER_LINUXSCHED == 0) 
	    schedule();
#  elif defined(CLICK_BSDMODULE)	/* BSD kernel module */
	if (iter % DRIVER_ITER_BSDSCHED == 0) {
	    extern int click_thread_priority;
	    int s = splhigh();
	    curproc->p_priority = curproc->p_usrpri = click_thread_priority;
	    setrunqueue(curproc);
	    mi_switch();
	    splx(s);
	}
#  else
#   error Compiling for unknown target.
#  endif  /* CLICK_LINUXMODULE */
# endif  /* CLICK_GREEDY */
#endif  /* !CLICK_USERLEVEL */
	nice_lock_tasks();
    }

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
