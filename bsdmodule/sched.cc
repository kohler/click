// -*- c-basic-offset: 4 -*-
/*
 * sched.cc -- BSD kernel scheduler thread for click
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2001-2003 International Computer Science Institute
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
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/kthread.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>


int click_thread_priority = PRIO_PROCESS;
static Vector<int> *click_thread_pids;
static Router *placeholder_router;

#ifdef HAVE_ADAPTIVE_SCHEDULER
static unsigned min_click_frac = 5, max_click_frac = 800;
#endif


static void
click_sched(void *thunk)
{
  curproc->p_nice = click_thread_priority;
  RouterThread *rt = (RouterThread *)thunk;
#ifdef HAVE_ADAPTIVE_SCHEDULER
  rt->set_cpu_share(min_click_frac, max_click_frac);
#endif

  // add pid to thread list
  if (click_thread_pids)
    click_thread_pids->push_back(curproc->p_pid);

  // driver loop; does not return for a while
  rt->driver();

  // release master (preserved in click_init_sched)
  click_master->unuse();

  // remove pid from thread list
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++) {
      if ((*click_thread_pids)[i] == curproc->p_pid) {
	(*click_thread_pids)[i] = click_thread_pids->back();
	click_thread_pids->pop_back();
	break;
      }
    }
  kthread_exit(0);
}

static int
kill_router_threads()
{
  if (click_router)
    click_router->set_runcount(-10000);
  delete placeholder_router;
  
  // wait up to 5 seconds for routers to exit
  unsigned long out_ticks = ticks + 5 * hz;
  int num_threads;
  do {
    num_threads = click_thread_pids->size();
    
    if (num_threads > 0)
      tsleep(curproc, PPAUSE, "unload", 1);
  } while (num_threads > 0 && ticks < out_ticks);

  if (num_threads > 0) {
    printf("click: current router threads refuse to die!\n");
    return -1;
  } else
    return 0;
}


/******************************* Handlers ************************************/

static String
read_threads(Element *, void *)
{
  StringAccum sa;
  simple_lock(&click_thread_lock);
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++)
      sa << (*click_thread_pids)[i] << '\n';
  simple_unlock(&click_thread_lock);
  return sa.take_string();
}

static String
read_priority(Element *, void *)
{
  return String(click_thread_priority) + "\n";
}

static int
write_priority(const String &conf, Element *, void *, ErrorHandler *errh)
{
  int priority;
  if (!cp_integer(cp_uncomment(conf), &priority))
    return errh->error("priority must be an integer");

  if (priority > PRIO_MAX)
    priority = PRIO_MAX;
  if (priority < PRIO_MIN)
    priority = PRIO_MIN;

  // change current thread priorities
  click_thread_priority = priority;
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++) {
      struct proc *proc = pfind((*click_thread_pids)[i]);
      if (proc) {
	proc->p_nice = priority;
	resetpriority(proc);
    }
  }
  
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
	if (!cp_unsigned(conf, &x)) 
	    return errh->error("tasks_per_iter must be unsigned\n");
	
	// change current thread priorities
	for (int i = 0; i < click_master->nthreads(); i++)
	    click_master->thread(i)->_tasks_per_iter = x;
    }

    case H_ITERS_PER_TIMERS: {
	unsigned x;
	if (!cp_unsigned(conf, &x)) 
	    return errh->error("tasks_per_iter_timers must be unsigned\n");
	
	// change current thread priorities
	for (int i = 0; i < click_master->nthreads(); i++)
	    click_master->thread(i)->_iters_per_timers = x;
    }

    case H_ITERS_PER_OS: {
	unsigned x;
	if (!cp_unsigned(conf, &x)) 
	    return errh->error("tasks_per_iter_os must be unsigned\n");
	
	// change current thread priorities
	for (int i = 0; i < click_master->nthreads(); i++)
	    click_master->thread(i)->_iters_per_os = x;
    }
    }
    return 0;
}

/********************** Initialization and cleanup ***************************/
#if __MTCLICK__
extern "C" int click_threads();
#endif

void
click_init_sched(ErrorHandler *errh)
{
  click_thread_pids = new Vector<int>;

#if __MTCLICK__
  click_master = new Master(click_threads());
  if (smp_num_cpus != NUM_CLICK_CPUS)
    click_chatter("warning: click compiled for %d cpus, machine allows %d", 
	          NUM_CLICK_CPUS, smp_num_cpus);
#else
  click_master = new Master(1);
#endif

  placeholder_router = new Router("", click_master);
  placeholder_router->initialize(errh);
  placeholder_router->activate(errh);

  for (int i = 0; i < click_master->nthreads(); i++) {
    struct proc *p;
    click_master->use();
    int error  = kthread_create
      (click_sched, click_master->thread(i), &p, "kclick");
    if (error < 0) {
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
    printf("click: Following threads still active, expect a crash:\n");
    for (int i = 0; i < click_thread_pids->size(); i++)
      printf("click:   router thread pid %d\n", (*click_thread_pids)[i]);
    return -1;
  } else {
    delete click_thread_pids;
    click_thread_pids = 0;
    return 0;
  }
}
