// -*- c-basic-offset: 4 -*-
/*
 * sched.cc -- kernel scheduler thread for click
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002-2003 International Computer Science Institute
 * Copyright (c) 2007 Regents of the University of California
 * Copyright (c) 1999-2013 Eddie Kohler
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

#define CLICK_SCHED_DEBUG
#include <click/config.h>
#include "modulepriv.hh"

#include <click/routerthread.hh>
#include <click/glue.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/master.hh>
#include <click/args.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/bitops.h>
#include <linux/cpumask.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
# include <linux/kthread.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#define MIN_PRIO	MAX_RT_PRIO
/* MAX_PRIO already defined */
#define PRIO2NICE(p)	((p) - MIN_PRIO - 20)
#define NICE2PRIO(n)	(MIN_PRIO + (n) + 20)
#define DEF_PRIO	NICE2PRIO(0)
#define TASK_PRIO(t)	((t)->static_prio)

#define SOFT_SPIN_LOCK(l)	do { /*MDEBUG("soft_lock %s", #l);*/ soft_spin_lock((l)); } while (0)
#define SPIN_UNLOCK(l)		do { /*MDEBUG("unlock %s", #l);*/ spin_unlock((l)); } while (0)

static spinlock_t click_thread_lock;
static int click_thread_priority = DEF_PRIO;
static Vector<struct task_struct *> *click_thread_tasks;
static Router *placeholder_router;

#if HAVE_ADAPTIVE_SCHEDULER
static unsigned min_click_frac = 5, max_click_frac = 800;
#endif

static void
soft_spin_lock(spinlock_t *l)
{
    while (!spin_trylock(l))
	schedule();
}

static int
click_sched(void *thunk)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
    /* daemonize seems to be unnecessary */
#else
    daemonize("kclick");
#endif

    TASK_PRIO(current) = click_thread_priority;

    RouterThread *rt = (RouterThread *)thunk;
#if HAVE_ADAPTIVE_SCHEDULER
    rt->set_cpu_share(min_click_frac, max_click_frac);
#endif

#ifdef CONFIG_SMP
    int mycpu = click_parm(CLICKPARM_CPU);
    if (mycpu >= 0) {
	mycpu += rt->thread_id();
	if (mycpu < num_possible_cpus() && cpu_online(mycpu)) {
# if CONFIG_CPUMASK_OFFSTACK
	    set_cpus_allowed_ptr(current, cpumask_of(mycpu));
# else
	    set_cpus_allowed(current, cpumask_of_cpu(mycpu));
# endif
	} else
	    printk(KERN_WARNING "click: warning: cpu %d for thread %d offline\n", mycpu, rt->thread_id());
    }
#endif

    printk(KERN_ALERT "click: starting router thread pid %d (%p)\n", current->pid, rt);

    // add pid to thread list
    SOFT_SPIN_LOCK(&click_thread_lock);
    if (click_thread_tasks)
	click_thread_tasks->push_back(current);
    SPIN_UNLOCK(&click_thread_lock);

    // driver loop; does not return for a while
    rt->driver();

    // release master (preserved in click_init_sched)
    click_master->unuse();

    // remove pid from thread list
    SOFT_SPIN_LOCK(&click_thread_lock);
    if (click_thread_tasks)
	for (int i = 0; i < click_thread_tasks->size(); i++) {
	    if ((*click_thread_tasks)[i] == current) {
		(*click_thread_tasks)[i] = click_thread_tasks->back();
		click_thread_tasks->pop_back();
		break;
	    }
	}
    printk(KERN_ALERT "click: stopping router thread pid %d\n", current->pid);
    SPIN_UNLOCK(&click_thread_lock);

    return 0;
}

static int
kill_router_threads()
{
    delete placeholder_router;
    if (click_router)
	click_router->set_runcount(Router::STOP_RUNCOUNT);

    // wait up to 5 seconds for routers to exit
    unsigned long out_jiffies = jiffies + 5 * HZ;
    int num_threads;
    do {
	MDEBUG("click_sched: waiting for threads to die");
	SOFT_SPIN_LOCK(&click_thread_lock);
	num_threads = click_thread_tasks->size();
	SPIN_UNLOCK(&click_thread_lock);
	if (num_threads > 0)
	    schedule();
    } while (num_threads > 0 && jiffies < out_jiffies);

    if (num_threads > 0) {
	printk(KERN_ALERT "click: current router threads refuse to die!\n");
	return -1;
    } else
	return 0;
}


/******************************* Handlers ************************************/

static String
read_threads(Element *, void *)
{
    StringAccum sa;
    MDEBUG("reading threads");
    SOFT_SPIN_LOCK(&click_thread_lock);
    if (click_thread_tasks)
	for (int i = 0; i < click_thread_tasks->size(); i++)
	    sa << (*click_thread_tasks)[i]->pid << '\n';
    SPIN_UNLOCK(&click_thread_lock);
    return sa.take_string();
}

static String
read_priority(Element *, void *)
{
    return String(PRIO2NICE(click_thread_priority));
}

static int
write_priority(const String &conf, Element *, void *, ErrorHandler *errh)
{
    int priority;
    if (!IntArg().parse(conf, priority))
	return errh->error("priority must be an integer");

    priority = NICE2PRIO(priority);
    if (priority < MIN_PRIO) {
	priority = MIN_PRIO;
	errh->warning("priority pinned at %d", PRIO2NICE(priority));
    } else if (priority > MAX_PRIO) {
	priority = MAX_PRIO;
	errh->warning("priority pinned at %d", PRIO2NICE(priority));
    }

    // change current thread priorities
    MDEBUG("writing priority");
    SOFT_SPIN_LOCK(&click_thread_lock);
    click_thread_priority = priority;
    if (click_thread_tasks)
	for (int i = 0; i < click_thread_tasks->size(); i++)
	    TASK_PRIO((*click_thread_tasks)[i]) = priority;
    SPIN_UNLOCK(&click_thread_lock);

    return 0;
}


#if CLICK_DEBUG_MASTER
static String
read_master_info(Element *, void *)
{
    return click_master->info();
}
#endif


#if HAVE_ADAPTIVE_SCHEDULER

static String
read_cpu_share(Element *, void *thunk)
{
    int val = (thunk ? max_click_frac : min_click_frac);
    return cp_unparse_real10(val, 3);
}

static String
read_cur_cpu_share(Element *, void *)
{
    if (click_router) {
	StringAccum sa;
	for (int i = 0; i < click_master->nthreads(); i++)
	    sa << cp_unparse_real10(click_master->thread(i)->cur_cpu_share(), 3)
	       << '\n';
	return sa.take_string();
    } else
	return "0\n";
}

static int
write_cpu_share(const String &conf, Element *, void *thunk, ErrorHandler *errh)
{
    const char *name = (thunk ? "max_" : "min_");

    int32_t frac;
    if (!cp_real10(conf, 3, &frac) || frac < 1 || frac > 999)
	return errh->error("%scpu_share must be a real number between 0.001 and 0.999", name);

    (thunk ? max_click_frac : min_click_frac) = frac;

    // change current thread priorities
    // XXX believed to be OK even if threads are currently running
    for (int i = 0; i < click_master->nthreads(); i++)
	click_master->thread(i)->set_cpu_share(min_click_frac, max_click_frac);

    return 0;
}

#endif

enum { H_TASKS_PER_ITER, H_ITERS_PER_TIMERS, H_ITERS_PER_OS };


static String
read_sched_param(Element *, void *thunk)
{
    String s;
    switch (reinterpret_cast<uintptr_t>(thunk)) {

      case H_TASKS_PER_ITER:
	for (int i = 0; i < click_master->nthreads(); i++)
	    s += String(click_master->thread(i)->_tasks_per_iter) + "\n";
	break;

      case H_ITERS_PER_TIMERS:
	for (int i = 0; i < click_master->nthreads(); i++)
	    s += String(click_master->thread(i)->timer_set().max_timer_stride()) + "\n";
	break;

      case H_ITERS_PER_OS:
	for (int i = 0; i < click_master->nthreads(); i++)
	    s += String(click_master->thread(i)->_iters_per_os) + "\n";
	break;

    }
    return s;
}

static int
write_sched_param(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
    switch (reinterpret_cast<uintptr_t>(thunk)) {

      case H_TASKS_PER_ITER: {
	  unsigned x;
	  if (!IntArg().parse(conf, x))
	      return errh->error("tasks_per_iter must be unsigned\n");
	  for (int i = 0; i < click_master->nthreads(); i++)
	      click_master->thread(i)->_tasks_per_iter = x;
	  break;
      }

      case H_ITERS_PER_TIMERS: {
	  unsigned x;
	  if (!IntArg().parse(conf, x))
	      return errh->error("tasks_per_iter_timers must be unsigned\n");
	  for (int i = 0; i < click_master->nthreads(); ++i)
	      click_master->thread(i)->timer_set().set_max_timer_stride(x);
	  break;
      }

      case H_ITERS_PER_OS: {
	  unsigned x;
	  if (!IntArg().parse(conf, x))
	      return errh->error("tasks_per_iter_os must be unsigned\n");
	  for (int i = 0; i < click_master->nthreads(); i++)
	      click_master->thread(i)->_iters_per_os = x;
	  break;
      }

    }
    return 0;
}

/********************** Initialization and cleanup ***************************/

void
click_init_sched(ErrorHandler *errh)
{
    spin_lock_init(&click_thread_lock);
    click_thread_tasks = new Vector<struct task_struct *>;
    bool greedy = click_parm(CLICKPARM_GREEDY);
#if HAVE_MULTITHREAD
    int threads = click_parm(CLICKPARM_THREADS);
    if (num_possible_cpus() < threads) {
	threads = num_possible_cpus();
	click_chatter(KERN_WARNING "warning: only %d cpus available, running only %d threads",
		      threads, threads);
    }
#else
    int threads = 1;
#endif

    click_master = new Master(threads);
    click_master->use();

    placeholder_router = new Router("", click_master);
    placeholder_router->initialize(errh);
    placeholder_router->activate(errh);

    for (int i = 0; i < click_master->nthreads(); i++) {
	click_master->use();
	RouterThread *thread = click_master->thread(i);
	thread->set_greedy(greedy);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
        struct task_struct* kthread = kthread_create
            (click_sched, thread, "kclick");
        if (!IS_ERR(kthread))
            wake_up_process(kthread);
        else {
            errh->error("cannot create kernel thread for Click thread %i!", i);
            click_master->unuse();
        }
#else
	pid_t pid = kernel_thread
	    (click_sched, thread, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
	if (pid < 0) {
	    errh->error("cannot create kernel thread for Click thread %i!", i);
	    click_master->unuse();
	}
#endif
    }

    Router::add_read_handler(0, "threads", read_threads, 0);
    Router::add_read_handler(0, "priority", read_priority, 0);
    Router::add_write_handler(0, "priority", write_priority, 0, Handler::h_nonexclusive);
#if HAVE_ADAPTIVE_SCHEDULER
    static_assert(Task::MAX_UTILIZATION == 1000, "The adaptive scheduler requires Task::MAX_UTILIZATION == 1000.");
    Router::add_read_handler(0, "min_cpu_share", read_cpu_share, 0);
    Router::add_write_handler(0, "min_cpu_share", write_cpu_share, 0, Handler::h_nonexclusive);
    Router::add_read_handler(0, "max_cpu_share", read_cpu_share, (void *)1);
    Router::add_write_handler(0, "max_cpu_share", write_cpu_share, (void *)1, Handler::h_nonexclusive);
    Router::add_read_handler(0, "cpu_share", read_cur_cpu_share, 0);
#else
    Router::add_read_handler(0, "tasks_per_iter", read_sched_param,
			     (void *)H_TASKS_PER_ITER);
    Router::add_read_handler(0, "iters_per_timers", read_sched_param,
			     (void *)H_ITERS_PER_TIMERS);
    Router::add_read_handler(0, "iters_per_os", read_sched_param,
			     (void *)H_ITERS_PER_OS);

    // XXX believed to be OK to run in parallel with thread processing
    Router::add_write_handler(0, "tasks_per_iter", write_sched_param,
			      (void *)H_TASKS_PER_ITER, Handler::h_nonexclusive);
    Router::add_write_handler(0, "iters_per_timers", write_sched_param,
			      (void *)H_ITERS_PER_TIMERS, Handler::h_nonexclusive);
    Router::add_write_handler(0, "iters_per_os", write_sched_param,
			      (void *)H_ITERS_PER_OS, Handler::h_nonexclusive);

#endif
#if CLICK_DEBUG_MASTER
    Router::add_read_handler(0, "master_info", read_master_info, 0);
#endif
}

int
click_cleanup_sched()
{
    if (kill_router_threads() < 0) {
	printk(KERN_ALERT "click: Following threads still active, expect a crash:\n");
	SOFT_SPIN_LOCK(&click_thread_lock);
	for (int i = 0; i < click_thread_tasks->size(); i++) {
	    struct task_struct *ct = (*click_thread_tasks)[i];
	    printk(KERN_ALERT "click:   router thread pid %d\n", (int) ct->pid);
	    printk(KERN_ALERT "click:   state %d, EIP %08lx\n", (int) ct->state, KSTK_EIP(ct));
	}
	SPIN_UNLOCK(&click_thread_lock);
	click_master->unuse();
	return -1;
    } else {
	delete click_thread_tasks;
	click_thread_tasks = 0;
	click_master->unuse();
	click_master = 0;
	return 0;
    }
}
