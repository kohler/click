/*
 * sched.cc -- kernel scheduler thread for click
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#define CLICK_SCHED_DEBUG

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "modulepriv.hh"
#include "kernelerror.hh"
#include <click/glue.hh>
#include <click/router.hh>
extern "C" {
#define __NO_VERSION__
#define new linux_new
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/bitops.h>
#undef new
}

spinlock_t click_thread_spinlock;
int click_thread_priority = DEF_PRIORITY;
Vector<int> *click_thread_pids;

static int
click_sched(void *thunk)
{
  Router *router = (Router *)thunk;
  current->session = 1;
  current->pgrp = 1;
  current->priority = click_thread_priority;
  sprintf(current->comm, "click");
  printk("<1>click: starting router %p\n", router);

  router->driver();

  router->unuse();

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
  
  printk("<1>click: stopping router %p\n", router);
  return 0;
}

int
start_click_sched(Router *r, ErrorHandler *kernel_errh)
{
  /* no thread if no router */
  if (r->nelements() == 0)
    return 0;

  spin_lock(&click_thread_spinlock);
  r->use();
  pid_t pid = kernel_thread
    (click_sched, r, CLONE_FS | CLONE_FILES | CLONE_SIGHAND); 
  if (pid < 0) {
    r->unuse();
    spin_unlock(&click_thread_spinlock);
    kernel_errh->error("cannot create kernel thread!"); 
    return -1;
  } else {
    if (click_thread_pids)
      click_thread_pids->push_back(pid);
    spin_unlock(&click_thread_spinlock);
    return 0;
  }
}

void
kill_click_sched(Router *r)
{
  r->please_stop_driver();
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
    printk("<1>click: %d threads are still active! Expect a crash!\n", num_threads);
    return -1;
  } else {
    delete click_thread_pids;
    click_thread_pids = 0;
    return 0;
  }
}
