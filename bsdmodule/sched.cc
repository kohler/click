/*
 * sched.cc -- BSD kernel scheduler thread for click
 * Benjie Chen
 * Nickolai Zeldovich (BSD port)
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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

#include "kernelerror.hh"
#include <click/routerthread.hh>
#include <click/glue.hh>
#include <click/router.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <sys/kthread.h>
#include <sys/proc.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

struct simplelock click_thread_lock;
Vector<int> *click_thread_pids;

static void
click_sched(void *thunk)
{
  RouterThread *rt = (RouterThread *)thunk;
  printf("click: starting router thread pid %d (%p)\n", curproc->p_pid, rt);

  rt->driver();
  rt->router()->unuse();

  simple_lock(&click_thread_lock);
  if (click_thread_pids) {
    for (int i = 0; i < click_thread_pids->size(); i++) {
      if ((*click_thread_pids)[i] == curproc->p_pid) {
	(*click_thread_pids)[i] = click_thread_pids->back();
	click_thread_pids->pop_back();
	break;
      }
    }
  }
  simple_unlock(&click_thread_lock);
  
  printf("click: stopping router thread pid %d\n", curproc->p_pid);
  kthread_exit(0);
}

int
start_click_sched(Router *r, int threads, ErrorHandler *kernel_errh)
{
  /* no thread if no router */
  if (r->nelements() == 0)
    return 0;

#ifdef __SMP__
  if (smp_num_cpus > NUM_CLICK_CPUS)
    click_chatter("warning: click compiled for %d cpus, machine allows %d", 
	          NUM_CLICK_CPUS, smp_num_cpus);
#endif

  simple_lock(&click_thread_lock);
  if (threads < 1)
    threads = 1;
  click_chatter((threads == 1 ? "starting %d thread" : "starting %d threads"), threads);

  while (threads > 0) {
    RouterThread *rt;
    if (threads > 1) 
      rt = new RouterThread(r);
    else
      rt = r->thread(0);
    r->use();
    struct proc *p;
    int status = kthread_create(click_sched, rt, &p, "click");
    if (status) {
      r->unuse();
      delete rt;
      simple_unlock(&click_thread_lock);
      kernel_errh->error("cannot create kernel thread!"); 
      return -1;
    } else {
      if (click_thread_pids)
        click_thread_pids->push_back(p->p_pid);
    }
    threads--;
  }

  simple_unlock(&click_thread_lock);
  return 0;
}

void
init_click_sched()
{
  simple_lock_init(&click_thread_lock);
  click_thread_pids = new Vector<int>;
}

int
cleanup_click_sched()
{
  // wait for up to 5 seconds for routers to exit
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
    printf("click: Following %d threads still active, expect a crash:\n",
	   num_threads);
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
