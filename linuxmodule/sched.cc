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
#include "router.hh"
#include "kernelerror.hh"

extern "C" {
void tulip_print_stats();
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

pid_t click_sched_pid = -1;  /* kernel thread ID for click scheduler */

extern "C" void start_click_sched(ErrorHandler *);
extern "C" void kill_click_sched();

void 
click_sched_die(int signo)
{
  extern Router *current_router;
  printk("click: caught signal %d\n", signo);
  current_router->please_stop_driver();
}

static int
click_sched(void *thunk)
{
  Router *router = (Router*) thunk;
  current->session = 1;
  current->pgrp = 1;
  sprintf(current->comm, "click");

  router->driver();

  click_sched_pid = -1;
  printk("click: router stopped, exiting!\n");
  return 0;
}

extern "C" void
start_click_sched(ErrorHandler *kernel_errh)
{
  extern Router *current_router;

  if (click_sched_pid > 0) { 
    kernel_errh->error("another click thread running.\n"); 
    return; 
  } 
  click_sched_pid = kernel_thread 
    (click_sched, current_router, CLONE_FS | CLONE_FILES | CLONE_SIGHAND); 
  if (click_sched_pid < 0) { 
    kernel_errh->error("cannot create kernel thread.\n"); 
    return; 
  }
}


extern "C" void
kill_click_sched()
{
  if (click_sched_pid > 0) { 
    kill_proc(click_sched_pid, SIGTERM, 1); 
    /* wait for thread to die - paranoid =) */ 
    while(click_sched_pid > 0) { 
      schedule(); 
      asm volatile ("" : : : "memory"); 
    } 
  }
  tulip_print_stats();
}
