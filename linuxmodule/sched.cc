/*
 * sched.cc -- kernel scheduler thread for click
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
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
#include <click/error.hh>
#include <click/straccum.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/bitops.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#ifdef LINUX_2_2
# define MIN_PRIO	1
# define MAX_PRIO	(2 * DEF_PRIORITY)
# define PRIO2NICE(p)	(DEF_PRIORITY - (p))
# define NICE2PRIO(n)	(DEF_PRIORITY - (n))
# define DEF_PRIO	DEF_PRIORITY
# define TASK_PRIO(t)	((t)->priority)
#else
# define MIN_PRIO	(-20)
# define MAX_PRIO	19
# define PRIO2NICE(p)	(p)
# define NICE2PRIO(n)	(n)
# define DEF_PRIO	DEF_NICE
# define TASK_PRIO(t)	((t)->nice)
#endif

#define SOFT_SPIN_LOCK(l)	soft_spin_lock((l))
#define SPIN_UNLOCK(l)		spin_unlock((l))

static spinlock_t click_thread_lock;
static int click_thread_priority = DEF_PRIO;
static Vector<int> *click_thread_pids;

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
  daemonize();
  
  TASK_PRIO(current) = click_thread_priority;
  strcpy(current->comm, "kclick");

  RouterThread *rt = (RouterThread *)thunk;
#ifdef HAVE_ADAPTIVE_SCHEDULER
  rt->set_cpu_share(min_click_frac, max_click_frac);
#endif
  printk("<1>click: starting router thread pid %d (%p)\n", current->pid, rt);

  // add pid to thread list
  SOFT_SPIN_LOCK(&click_thread_lock);
  if (click_thread_pids)
    click_thread_pids->push_back(current->pid);
  SPIN_UNLOCK(&click_thread_lock);

  // driver loop; does not return for a while
  rt->driver();

  // release router (router preserved in click_start_sched)
  rt->router()->unuse();

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

int
click_start_sched(Router *r, int threads, ErrorHandler *errh)
{
  // no thread if no router
  if (r->nelements() == 0)
    return 0;

#ifdef __SMP__
  if (smp_num_cpus > NUM_CLICK_CPUS)
    click_chatter("warning: click compiled for %d cpus, machine allows %d", 
	          NUM_CLICK_CPUS, smp_num_cpus);
#endif

  if (threads < 1)
    threads = 1;
  click_chatter((threads == 1 ? "starting %d thread" : "starting %d threads"), threads);

  while (threads > 0) {
    RouterThread *rt;
    if (threads > 1) 
      rt = new RouterThread(r);
    else
      rt = r->thread(0);

    r->use();			// preserve router
    pid_t pid = kernel_thread 
      (click_sched, rt, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
    
    if (pid < 0) {
      r->unuse();		// release router (no thread)
      delete rt;
      errh->error("cannot create kernel thread!"); 
      return -1;
    }
    
    threads--;
  }

  return 0;
}

int
click_kill_router_threads()
{
  if (click_router)
    click_router->please_stop_driver();
  
  // wait up to 5 seconds for routers to exit
  unsigned long out_jiffies = jiffies + 5 * HZ;
  int num_threads;
  do {
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
    for (int i = 0; i < click_router->nthreads(); i++)
      s += cp_unparse_real10(click_router->thread(i)->cur_cpu_share(), 3) + "\n";
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
    return errh->error("%scpu_share must be a real number between 0.001 and 0.999");

  (thunk ? max_click_frac : min_click_frac) = frac;

  // change current thread priorities
  if (click_router)
    for (int i = 0; i < click_router->nthreads(); i++)
      click_router->thread(i)->set_cpu_share(min_click_frac, max_click_frac);
  
  return 0;
}

#endif


/********************** Initialization and cleanup ***************************/

void
click_init_sched()
{
  spin_lock_init(&click_thread_lock);
  click_thread_pids = new Vector<int>;
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
#endif
}

int
click_cleanup_sched()
{
  if (click_kill_router_threads() < 0) {
    printk("<1>click: Following threads still active, expect a crash:\n");
    SOFT_SPIN_LOCK(&click_thread_lock);
    for (int i = 0; i < click_thread_pids->size(); i++)
      printk("<1>click:   router thread pid %d\n", (*click_thread_pids)[i]);
    SPIN_UNLOCK(&click_thread_lock);
    return -1;
  } else {
    delete click_thread_pids;
    click_thread_pids = 0;
    return 0;
  }
}
