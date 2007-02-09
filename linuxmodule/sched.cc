// -*- c-basic-offset: 4 -*-
/*
 * sched.cc -- kernel scheduler thread for click
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002-2003 International Computer Science Institute
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

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/bitops.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# include <linux/cpumask.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# define MIN_PRIO	MAX_RT_PRIO
/* MAX_PRIO already defined */
# define PRIO2NICE(p)	((p) - MIN_PRIO - 20)
# define NICE2PRIO(n)	(MIN_PRIO + (n) + 20)
# define DEF_PRIO	NICE2PRIO(0)
# define TASK_PRIO(t)	((t)->static_prio)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
# define MIN_PRIO	(-20)
# define MAX_PRIO	20
# define PRIO2NICE(p)	(p)
# define NICE2PRIO(n)	(n)
# define DEF_PRIO	DEF_NICE
# define TASK_PRIO(t)	((t)->nice)
#else
# define MIN_PRIO	1
# define MAX_PRIO	(2 * DEF_PRIORITY)
# define PRIO2NICE(p)	(DEF_PRIORITY - (p))
# define NICE2PRIO(n)	(DEF_PRIORITY - (n))
# define DEF_PRIO	DEF_PRIORITY
# define TASK_PRIO(t)	((t)->priority)
#endif

#define SOFT_SPIN_LOCK(l)	do { /*MDEBUG("soft_lock %s", #l);*/ soft_spin_lock((l)); } while (0)
#define SPIN_UNLOCK(l)		do { /*MDEBUG("unlock %s", #l);*/ spin_unlock((l)); } while (0)

static spinlock_t click_thread_lock;
static int click_thread_priority = DEF_PRIO;
static Vector<int> *click_thread_pids;
static Router *placeholder_router;

#ifdef HAVE_ADAPTIVE_SCHEDULER
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
#ifdef LINUX_2_2
  // In Linux 2.2, daemonize() doesn't do exit_files.
  exit_files(current);
  current->files = init_task.files;
  if (current->files)
    atomic_inc(&current->files->count);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
  daemonize("kclick");
#else
  daemonize();
  strcpy(current->comm, "kclick");
#endif
  
  TASK_PRIO(current) = click_thread_priority;

  RouterThread *rt = (RouterThread *)thunk;
#ifdef HAVE_ADAPTIVE_SCHEDULER
  rt->set_cpu_share(min_click_frac, max_click_frac);
#endif

#if CONFIG_SMP
  int mycpu = click_parm(CLICKPARM_CPU);
  if (mycpu >= 0 && click_parm(CLICKPARM_THREADS) <= 1) {
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
      if (mycpu < num_possible_cpus() && cpu_online(mycpu))
	  set_cpus_allowed(current, cpumask_of_cpu(mycpu));
      else
	  printk("<1>click: warning: cpu %d offline\n", mycpu);
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 21)
      if (mycpu < smp_num_cpus && (cpu_online_map & (1UL << cpu_logical_map(mycpu))))
	  set_cpus_allowed(current, 1UL << cpu_logical_map(mycpu));
      else
	  printk("<1>click: warning: cpu %d offline\n", mycpu);
# endif
  }
#endif
  
  printk("<1>click: starting router thread pid %d (%p)\n", current->pid, rt);

  // add pid to thread list
  SOFT_SPIN_LOCK(&click_thread_lock);
  if (click_thread_pids)
    click_thread_pids->push_back(current->pid);
  SPIN_UNLOCK(&click_thread_lock);

  // driver loop; does not return for a while
  rt->driver();

  // release master (preserved in click_init_sched)
  click_master->unuse();

  // remove pid from thread list
  SOFT_SPIN_LOCK(&click_thread_lock);
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++) {
      if ((*click_thread_pids)[i] == current->pid) {
	(*click_thread_pids)[i] = click_thread_pids->back();
	click_thread_pids->pop_back();
	break;
      }
    }
  printk("<1>click: stopping router thread pid %d\n", current->pid);
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
	num_threads = click_thread_pids->size();
	SPIN_UNLOCK(&click_thread_lock);
	if (num_threads > 0)
	    schedule();
    } while (num_threads > 0 && jiffies < out_jiffies);

  if (num_threads > 0) {
    printk("<1>click: current router threads refuse to die!\n");
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
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++)
      sa << (*click_thread_pids)[i] << '\n';
  SPIN_UNLOCK(&click_thread_lock);
  return sa.take_string();
}

static String
read_priority(Element *, void *)
{
  return String(PRIO2NICE(click_thread_priority)) + "\n";
}

static int
write_priority(const String &conf, Element *, void *, ErrorHandler *errh)
{
  int priority;
  if (!cp_integer(cp_uncomment(conf), &priority))
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
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++) {
      struct task_struct *task = find_task_by_pid((*click_thread_pids)[i]);
      if (task)
	TASK_PRIO(task) = priority;
    }
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


#ifdef HAVE_ADAPTIVE_SCHEDULER

static String
read_cpu_share(Element *, void *thunk)
{
  int val = (thunk ? max_click_frac : min_click_frac);
  return cp_unparse_real10(val, 3) + "\n";
}

static String
read_cur_cpu_share(Element *, void *)
{
  if (click_router) {
    String s;
    for (int i = 0; i < click_master->nthreads(); i++)
      s += cp_unparse_real10(click_master->thread(i)->cur_cpu_share(), 3) + "\n";
    return s;
  } else
    return "0\n";
}

static int
write_cpu_share(const String &conf, Element *, void *thunk, ErrorHandler *errh)
{
  const char *name = (thunk ? "max_" : "min_");
  
  int32_t frac;
  if (!cp_real10(cp_uncomment(conf), 3, &frac) || frac < 1 || frac > 999)
    return errh->error("%scpu_share must be a real number between 0.001 and 0.999", name);

  (thunk ? max_click_frac : min_click_frac) = frac;

  // change current thread priorities
  for (int i = 0; i < click_master->nthreads(); i++)
    click_master->thread(i)->set_cpu_share(min_click_frac, max_click_frac);
  
  return 0;
}

#endif

enum { H_TASKS_PER_ITER, H_ITERS_PER_TIMERS, H_ITERS_PER_OS };


static String
read_sched_param(Element *, void *thunk) 
{
    switch ((int)thunk) {
    case H_TASKS_PER_ITER: {
	if (click_router) {
	    String s;
	    for (int i = 0; i < click_master->nthreads(); i++)
		s += String(click_master->thread(i)->_tasks_per_iter) + "\n";
	return s;
	}
    }

    case H_ITERS_PER_TIMERS: {
	if (click_router) {
	    String s;
	    for (int i = 0; i < click_master->nthreads(); i++)
		s += String(click_master->thread(i)->_iters_per_timers) + "\n";
	return s;
	}
    }
    case H_ITERS_PER_OS: {
	if (click_router) {
	    String s;
	    for (int i = 0; i < click_master->nthreads(); i++)
		s += String(click_master->thread(i)->_iters_per_os) + "\n";
	return s;
	}
    }
    }
    return String("0\n");

}
static int
write_sched_param(const String &conf, Element *e, void *thunk, ErrorHandler *errh) 
{

    switch((int)thunk) {

    case H_TASKS_PER_ITER: {
	unsigned x;
	if (!cp_integer(conf, &x)) 
	    return errh->error("tasks_per_iter must be unsigned\n");
	
	// change current thread priorities
	for (int i = 0; i < click_master->nthreads(); i++)
	    click_master->thread(i)->_tasks_per_iter = x;
    }

    case H_ITERS_PER_TIMERS: {
	unsigned x;
	if (!cp_integer(conf, &x)) 
	    return errh->error("tasks_per_iter_timers must be unsigned\n");
	
	// change current thread priorities
	for (int i = 0; i < click_master->nthreads(); i++)
	    click_master->thread(i)->_iters_per_timers = x;
    }

    case H_ITERS_PER_OS: {
	unsigned x;
	if (!cp_integer(conf, &x)) 
	    return errh->error("tasks_per_iter_os must be unsigned\n");
	
	// change current thread priorities
	for (int i = 0; i < click_master->nthreads(); i++)
	    click_master->thread(i)->_iters_per_os = x;
    }
    }
    return 0;
}

/********************** Initialization and cleanup ***************************/

void
click_init_sched(ErrorHandler *errh)
{
  spin_lock_init(&click_thread_lock);
  click_thread_pids = new Vector<int>;
  bool greedy = click_parm(CLICKPARM_GREEDY);

#if __MTCLICK__
  click_master = new Master(click_parm(CLICKPARM_THREADS));
  if (num_possible_cpus() != NUM_CLICK_CPUS)
    click_chatter("warning: click compiled for %d cpus, machine allows %d", 
	          NUM_CLICK_CPUS, num_possible_cpus());
#else
  click_master = new Master(1);
#endif
  click_master->use();

  placeholder_router = new Router("", click_master);
  placeholder_router->initialize(errh);
  placeholder_router->activate(errh);

  for (int i = 0; i < click_master->nthreads(); i++) {
    click_master->use();
    RouterThread *thread = click_master->thread(i);
    thread->set_greedy(greedy);
    pid_t pid = kernel_thread 
      (click_sched, thread, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
    if (pid < 0) {
      errh->error("cannot create kernel thread for Click thread %i!", i); 
      click_master->unuse();
    }
  }

  Router::add_read_handler(0, "threads", read_threads, 0);
  Router::add_read_handler(0, "priority", read_priority, 0);
  Router::add_write_handler(0, "priority", write_priority, 0);
#ifdef HAVE_ADAPTIVE_SCHEDULER
  static_assert(Task::MAX_UTILIZATION == 1000);
  Router::add_read_handler(0, "min_cpu_share", read_cpu_share, 0);
  Router::add_write_handler(0, "min_cpu_share", write_cpu_share, 0);
  Router::add_read_handler(0, "max_cpu_share", read_cpu_share, (void *)1);
  Router::add_write_handler(0, "max_cpu_share", write_cpu_share, (void *)1);
  Router::add_read_handler(0, "cpu_share", read_cur_cpu_share, 0);
#else 
  Router::add_read_handler(0, "tasks_per_iter", read_sched_param, 
			   (void *)H_TASKS_PER_ITER);
  Router::add_read_handler(0, "iters_per_timers", read_sched_param, 
			   (void *)H_ITERS_PER_TIMERS);
  Router::add_read_handler(0, "iters_per_os", read_sched_param, 
			   (void *)H_ITERS_PER_OS);

  Router::add_write_handler(0, "tasks_per_iter", write_sched_param, 
			    (void *)H_TASKS_PER_ITER);
  Router::add_write_handler(0, "iters_per_timers", write_sched_param, 
			    (void *)H_ITERS_PER_TIMERS);
  Router::add_write_handler(0, "iters_per_os", write_sched_param, 
			    (void *)H_ITERS_PER_OS);

#endif
#if CLICK_DEBUG_MASTER
  Router::add_read_handler(0, "master_info", read_master_info, 0);
#endif
}

int
click_cleanup_sched()
{
  if (kill_router_threads() < 0) {
    printk("<1>click: Following threads still active, expect a crash:\n");
    SOFT_SPIN_LOCK(&click_thread_lock);
    for (int i = 0; i < click_thread_pids->size(); i++) {
      printk("<1>click:   router thread pid %d\n", (*click_thread_pids)[i]);
      struct task_struct *ct = find_task_by_pid((*click_thread_pids)[i]);
      if (ct)
	  printk("<1>click:   state %d, EIP %08x\n", (int) ct->state, KSTK_EIP(ct));
    }
    SPIN_UNLOCK(&click_thread_lock);
    click_master->unuse();
    return -1;
  } else {
    delete click_thread_pids;
    click_thread_pids = 0;
    click_master->unuse();
    click_master = 0;
    return 0;
  }
}
