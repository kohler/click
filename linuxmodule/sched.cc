/*
 * sched.cc -- kernel scheduler for click
 * Benjie Chen
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CLICK_SCHEDULER

#define CLICK_SCHED_DEBUG


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

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
#include <asm/io.h>
#undef new
}

static pid_t click_sched_pid;      /* kernel thread ID for click scheduler */
static struct wait_queue *click_sched_wq = NULL;   /* killing sched thread */

extern "C" void click_start_sched();
extern "C" void click_kill_sched();


static int
click_sched(void *)
{ 
    extern struct device *tulip_dev; /* XXX - hack */ 
    struct device *dev = tulip_dev;
    int total_poll_work = 0;
    int total_intr_wait = 0;
    int idle = 0;

    current->session = 1;
    current->pgrp = 1;
    sprintf(current->comm, "click_sched");

    dev->intr_off(dev); 
    dev->poll_mode = 1;

    for(;;) 
    {
    	int work = tulip_poll();

    	if (work > 0) 
    	{
		total_poll_work += work;
		do_bottom_half();
    	}
	else 
	{
	    idle++; 
	    if (signal_pending(current)) 
	    { 
		printk("click_sched: received signal, quiting\n"); 
#if 0
	        if (sigismember(&current->signal,SIGKILL))
#endif
	        { 
		    /* shutdown */ 
		    wake_up(&click_sched_wq); 
		    break; 
		}
#if 0
	        flush_signals(current);
#endif
            } 
	}

	if (idle == 25)
	{
	    total_intr_wait++;
	    dev->intr_on(dev);
      	    interruptible_sleep_on(&dev->intr_wq); 
	    dev->intr_off(dev);
	    idle = 0;
	}
    }

    printk("click_sched: waited for interrupts %d times\n", total_intr_wait);
    dev->poll_mode = 0;
    dev->intr_on(dev); 
    return 0;
}

    
extern "C" void
click_start_sched()
{
    click_sched_pid = kernel_thread
	(click_sched, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
    if (click_sched_pid < 0)
	printk("click_sched: cannot create kernel thread, not running.\n");
}


extern "C" void
click_kill_sched()
{
    if (click_sched_pid > 0)
    {
	kill_proc(click_sched_pid, SIGKILL, 1);
	interruptible_sleep_on(&click_sched_wq);
    }
}


#endif HAVE_CLICK_SCHEDULER

