/*
 * sched.cc -- kernel scheduler thread for click
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#define CLICK_SCHED_DEBUG

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "routerthread.hh"
#include "router.hh"
#include "kernelerror.hh"

extern "C" {
#define __NO_VERSION__
#define new linux_new
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/malloc.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/bitops.h>
#undef new
}

atomic_t num_click_threads;

static int
click_sched(void *thunk)
{
  RouterThread *rt = (RouterThread *)thunk;
  current->session = 1;
  current->pgrp = 1;
  sprintf(current->comm, "click");
  printk("<1>click: starting router thread %p\n", rt);

  rt->driver();
  delete rt;

  atomic_dec(&num_click_threads);
  printk("<1>click: stopping router thread %p\n", rt);
  return 0;
}

int
start_click_sched(Router *r, int threads, ErrorHandler *kernel_errh)
{
  /* no thread if no router */
  if (r->nelements() == 0)
    return 0;
  click_chatter("starting %d threads", threads); 
  while (threads > 0) {
    atomic_inc(&num_click_threads);
    RouterThread *rt = new RouterThread(r);
    pid_t pid = kernel_thread 
      (click_sched, rt, CLONE_FS | CLONE_FILES | CLONE_SIGHAND); 
    if (pid < 0) {
      delete rt;
      atomic_dec(&num_click_threads);
      kernel_errh->error("cannot create kernel thread!"); 
      return -1;
    }
    threads--;
  }
  return 0;
}

void
init_click_sched()
{
  atomic_set(&num_click_threads, 0);
}

int
cleanup_click_sched()
{
  // wait for up to 5 seconds for routers to exit
  unsigned long out_jiffies = jiffies + 5 * HZ;
  while (atomic_read(&num_click_threads) > 0 && jiffies < out_jiffies)
    schedule();

  if (atomic_read(&num_click_threads) > 0) {
    printk("<1>click: %d threads are still active! Expect a crash!\n",
	   atomic_read(&num_click_threads));
    return -1;
  } else
    return 0;
}
