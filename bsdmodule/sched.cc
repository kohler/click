/*
 * sched.cc -- BSD kernel scheduler thread for click
 * Benjie Chen, Nickolai Zeldovich (BSD port), Eddie Kohler
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include <sys/kthread.h>
#include <sys/proc.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

static simplelock click_thread_lock;
int click_thread_priority = MAXPRI;
static Vector<int> *click_thread_pids;

static void
click_sched(void *thunk)
{
  curproc->p_priority = curproc->p_usrpri = click_thread_priority;
  curproc->p_nice = 0;
  curproc->p_rtprio.type = RTP_PRIO_NORMAL;
  curproc->p_rtprio.prio = RTP_PRIO_MAX;
  // XXX strcpy(current->comm, "kclick");

  RouterThread *rt = (RouterThread *)thunk;
  printf("click: starting router thread pid %d (%p)\n", curproc->p_pid, rt);

  // add pid to thread list
  simple_lock(&click_thread_lock);
  if (click_thread_pids)
    click_thread_pids->push_back(curproc->p_pid);
  simple_unlock(&click_thread_lock);

  // preserve router
  rt->router()->use();

  // driver loop; does not return for a while
  rt->driver();

  // release router
  rt->router()->unuse();

  // remove pid from thread list
  simple_lock(&click_thread_lock);
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++) {
      if ((*click_thread_pids)[i] == curproc->p_pid) {
	(*click_thread_pids)[i] = click_thread_pids->back();
	click_thread_pids->pop_back();
	break;
      }
    }
  simple_unlock(&click_thread_lock);
  
  printf("click: stopping router thread pid %d\n", curproc->p_pid);
  kthread_exit(0);
}

int
click_start_sched(Router *r, int threads, ErrorHandler *errh)
{
  // no thread if no router
  if (r->nelements() == 0)
    return 0;

  if (threads < 1)
    threads = 1;
  click_chatter((threads == 1 ? "starting %d thread" : "starting %d threads"), threads);

  while (threads > 0) {
    RouterThread *rt;
    if (threads > 1) 
      rt = new RouterThread(r);
    else
      rt = r->thread(0);
    struct proc *p;
    int status = kthread_create(click_sched, rt, &p, "kclick");
    if (status) {
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
  unsigned long out_jiffies = click_jiffies() + 5 * CLICK_HZ;
  int num_threads;
  do {
    simple_lock(&click_thread_lock);
    num_threads = click_thread_pids->size();
    simple_unlock(&click_thread_lock);
    if (num_threads > 0)
      yield(curproc, NULL);
  } while (num_threads > 0 && click_jiffies() < out_jiffies);

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

#if 0
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
    return errh->error("priority must be integer");

  priority = NICE2PRIO(priority);
  if (priority < MIN_PRIO) {
    priority = MIN_PRIO;
    errh->warning("priority pinned at %d", PRIO2NICE(priority));
  } else if (priority > MAX_PRIO) {
    priority = MAX_PRIO;
    errh->warning("priority pinned at %d", PRIO2NICE(priority));
  }

  // change current thread priorities
  soft_spin_lock(&click_thread_lock);
  click_thread_priority = priority;
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++) {
      struct task_struct *task = find_task_by_pid((*click_thread_pids)[i]);
      if (task)
	TASK_PRIO(task) = priority;
    }
  spin_unlock(&click_thread_lock);
  
  return 0;
}
#endif


/********************** Initialization and cleanup ***************************/

void
click_init_sched()
{
  simple_lock_init(&click_thread_lock);
  click_thread_pids = new Vector<int>;
  Router::add_global_read_handler("threads", read_threads, 0);
#if 0
  Router::add_global_read_handler("priority", read_priority, 0);
  Router::add_global_write_handler("priority", write_priority, 0);
#endif
}

int
click_cleanup_sched()
{
  if (click_kill_router_threads() < 0) {
    printf("click: Following threads still active, expect a crash:\n");
    simple_lock(&click_thread_lock);
    for (int i = 0; i < click_thread_pids->size(); i++)
      printf("click:   router thread pid %d\n", (*click_thread_pids)[i]);
    simple_unlock(&click_thread_lock);
    return -1;
  } else {
    delete click_thread_pids;
    click_thread_pids = 0;
    return 0;
  }
}
