/*
 * sched.cc -- kernel scheduler thread for click
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/bitops.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#ifdef LINUX_2_2
# define DEF_PRIO	DEF_PRIORITY
# define TASK_PRIO(t)	((t)->priority)
#else
# define DEF_PRIO	DEF_NICE
# define TASK_PRIO(t)	((t)->nice)
#endif

spinlock_t click_thread_spinlock;
int click_thread_priority = DEF_PRIO;
Vector<int> *click_thread_pids;

static int
click_sched(void *thunk)
{
  RouterThread *rt = (RouterThread *)thunk;
  current->session = 1;
  current->pgrp = 1;
  TASK_PRIO(current) = click_thread_priority;
  sprintf(current->comm, "click");
  printk("<1>click: starting router thread pid %d (%p)\n", current->pid, rt);

  rt->driver();
  
  rt->router()->unuse();

  spin_lock(&click_thread_spinlock);
  if (click_thread_pids) {
    for (int i = 0; i < click_thread_pids->size(); i++)
      if ((*click_thread_pids)[i] == current->pid) {
	(*click_thread_pids)[i] = click_thread_pids->back();
	click_thread_pids->pop_back();
	break;
      }
  }
  spin_unlock(&click_thread_spinlock);
  
  printk("<1>click: stopping router thread pid %d\n", current->pid);
  return 0;
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

  spin_lock(&click_thread_spinlock);
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
    pid_t pid = kernel_thread 
      (click_sched, rt, CLONE_FS | CLONE_FILES | CLONE_SIGHAND); 
    if (pid < 0) {
      r->unuse();
      delete rt;
      spin_unlock(&click_thread_spinlock);
      kernel_errh->error("cannot create kernel thread!"); 
      return -1;
    } else {
      if (click_thread_pids)
        click_thread_pids->push_back(pid);
    }
    threads--;
  }

  spin_unlock(&click_thread_spinlock);
  return 0;
}

void
init_click_sched()
{
  spin_lock_init(&click_thread_spinlock);
  click_thread_pids = new Vector<int>;
}

int
cleanup_click_sched()
{
  // wait for up to 5 seconds for routers to exit
  unsigned long out_jiffies = jiffies + 5 * HZ;
  int num_threads;
  do {
    spin_lock(&click_thread_spinlock);
    num_threads = click_thread_pids->size();
    spin_unlock(&click_thread_spinlock);
    if (num_threads > 0)
      schedule();
  } while (num_threads > 0 && jiffies < out_jiffies);

  if (num_threads > 0) {
    printk("<1>click: Following threads still active, expect a crash:\n", num_threads);
    spin_lock(&click_thread_spinlock);
    for (int i = 0; i < click_thread_pids->size(); i++)
      printk("<1>click:   router thread pid %d\n", (*click_thread_pids)[i]);
    spin_unlock(&click_thread_spinlock);
    return -1;
  } else {
    delete click_thread_pids;
    click_thread_pids = 0;
    return 0;
  }
}
