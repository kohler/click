// -*- c-basic-offset: 4 -*-
/*
 * sched.cc -- BSD kernel scheduler thread for click
 * Benjie Chen, Eddie Kohler, Marko Zec
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2001-2004 International Computer Science Institute
 * Copyright (c) 2004 University of Zagreb
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
#include <click/args.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/master.hh>
#include <click/confparse.hh>

#ifdef BSD_NETISRSCHED
#undef DEVICE_POLLING
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
#include <elements/bsdmodule/anydevice.hh>
#else /* !BSD_NETISRSCHED */
#endif

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <net/netisr.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

CLICK_USING_DECLS

static Router *placeholder_router;

#ifdef BSD_NETISRSCHED
struct ifnet click_dummyifnet;
struct callout click_timer_h;

# if __FreeBSD_version >= 800000
void click_netisr(struct mbuf*);
static struct netisr_handler click_nh = {
  "click",
  click_netisr,
  NULL,
  NULL,
  NULL,
  NETISR_CLICK,
  0,
  NETISR_POLICY_SOURCE,
};

#  define MT_CLICK 13 /* XXX */
static struct mbuf *click_timer_mbuf;
# endif
#else  //BSD_NETISRSCHED

int click_thread_priority = PRIO_PROCESS;
static Vector<int> *click_thread_pids;

#if HAVE_ADAPTIVE_SCHEDULER
static unsigned min_click_frac = 5, max_click_frac = 800;
#endif

static struct mtx click_thread_lock;

static void
click_sched(void *thunk)
{
  curproc->p_nice = click_thread_priority;
  RouterThread *rt = (RouterThread *)thunk;
#if HAVE_ADAPTIVE_SCHEDULER
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
# if __FreeBSD_version >= 800000
  kthread_exit();
# else
  kthread_exit(0);
# endif
}

static int
kill_router_threads()
{
  if (click_router)
      click_router->set_runcount(Router::STOP_RUNCOUNT);
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
#endif //BSD_NETISRSCHED


/******************************* Handlers ************************************/

#ifndef BSD_NETISRSCHED

static String
read_threads(Element *, void *)
{
  StringAccum sa;
  mtx_lock(&click_thread_lock);
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++)
      sa << (*click_thread_pids)[i] << '\n';
  mtx_unlock(&click_thread_lock);
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
  if (!IntArg().parse(conf, priority))
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
# if 0
        /* XXX: FreeBSD does not have resetpriority(proc) */
	resetpriority(proc);
# endif
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


#if HAVE_ADAPTIVE_SCHEDULER

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
  if (!cp_real10(conf, 3, &frac) || frac < 1 || frac > 999)
    return errh->error("%scpu_share must be a real number between 0.001 and 0.999", name);

  (thunk ? max_click_frac : min_click_frac) = frac;

  // change current thread priorities
  for (int i = 0; i < click_master->nthreads(); i++)
    click_master->thread(i)->set_cpu_share(min_click_frac, max_click_frac);

  return 0;
}

#endif

#endif //BSD_NETISRSCHED

enum { H_TASKS_PER_ITER, H_ITERS_PER_TIMERS, H_ITERS_PER_OS };


static String
read_sched_param(Element *, void *thunk)
{
    switch ((intptr_t) thunk) {
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
		s += String(click_master->thread(i)->timer_set().max_timer_stride()) + "\n";
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
    switch ((intptr_t) thunk) {

    case H_TASKS_PER_ITER: {
	unsigned x;
	if (!IntArg().parse(conf, x))
	    return errh->error("tasks_per_iter must be unsigned\n");

	// change current thread priorities
	for (int i = 0; i < click_master->nthreads(); i++)
	    click_master->thread(i)->_tasks_per_iter = x;
    }

    case H_ITERS_PER_TIMERS: {
	unsigned x;
	if (!IntArg().parse(conf, x))
	    return errh->error("tasks_per_iter_timers must be unsigned\n");

	// change current thread priorities
	for (int i = 0; i < click_master->nthreads(); ++i)
	    click_master->thread(i)->timer_set().set_max_timer_stride(x);
    }

    case H_ITERS_PER_OS: {
	unsigned x;
	if (!IntArg().parse(conf, x))
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

#ifdef BSD_NETISRSCHED

#if 0
static void
click_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
  schednetisr(NETISR_CLICK);
}
#endif

static void
click_timer(void *arg)
{
#if 0
  if (polling && *polling)
    ether_poll_register(click_poll, &click_dummyifnet);
  else
#endif
#if __FreeBSD_version >= 800000
# ifdef BSD_NETISRSCHED
  /*
   * XXX: FreeBSD 8 does not have schednetisr().
   *      Calling netisr_dispatch() here is a hack.
   */
  netisr_dispatch(NETISR_CLICK, click_timer_mbuf);
# endif
#else
  schednetisr(NETISR_CLICK);
#endif
  callout_reset(&click_timer_h, 1, click_timer, NULL);
}


void
click_netisr(struct mbuf* buf)
{
  RouterThread *rt = click_master->thread(0);
  rt->driver();
}

#endif //BSD_NETISRSCHED


void
click_init_sched(ErrorHandler *errh)
{
#ifndef BSD_NETISRSCHED
  click_thread_pids = new Vector<int>;

  mtx_init(&click_thread_lock, "click thread lock", NULL, MTX_SPIN); /* XXX */
#endif

  click_master = new Master(1);

#ifdef BSD_NETISRSCHED
# if __FreeBSD_version >= 800000
  netisr_register(&click_nh);
  click_timer_mbuf = m_get(M_DONTWAIT, MT_CLICK); /* XXX */
# else
  netisr_register(NETISR_CLICK, click_netisr, NULL, 0);
  schednetisr(NETISR_CLICK);
# endif
  callout_init(&click_timer_h, 0);
  callout_reset(&click_timer_h, 1, click_timer, NULL);
  click_dummyifnet.if_flags |= IFF_UP|IFF_DRV_RUNNING;
#endif

  placeholder_router = new Router("", click_master);
  placeholder_router->initialize(errh);
  placeholder_router->activate(errh);

#ifndef BSD_NETISRSCHED
  for (int i = 0; i < click_master->nthreads(); i++) {
    struct proc *p;
    click_master->use();
# if __FreeBSD_version >= 800000
    struct thread *newtd;
    int error  = kthread_add
      (click_sched, click_master->thread(i), p, &newtd, 0, 0, "kclick"); /*XXX*/
# else
    int error  = kthread_create
      (click_sched, click_master->thread(i), &p, 0, 0, "kclick");
# endif
    if (error < 0) {
      errh->error("cannot create kernel thread for Click thread %i!", i);
      click_master->unuse();
    }
  }

  Router::add_read_handler(0, "threads", read_threads, 0);
  Router::add_read_handler(0, "priority", read_priority, 0);
  Router::add_write_handler(0, "priority", write_priority, 0);
#endif //BSD_NETISRSCHED
#if HAVE_ADAPTIVE_SCHEDULER
  static_assert(Task::MAX_UTILIZATION == 1000, "The adaptive scheduler requires TASK::MAX_UTILIZATION == 1000.");
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
#ifdef BSD_NETISRSCHED
  callout_drain(&click_timer_h);
# if __FreeBSD_version >= 800000
  netisr_unregister(&click_nh);
  m_free(click_timer_mbuf);
# else
  netisr_unregister(NETISR_CLICK);
# endif
# if 0
  ether_poll_deregister(&click_dummyifnet);
# endif
  delete placeholder_router;
#else
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
  mtx_destroy(&click_thread_lock);
#endif
   return 0;
}
