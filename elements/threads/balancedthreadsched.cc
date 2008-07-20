// -*- c-basic-offset: 4 -*-
/*
 * balancedthreadsched.{cc,hh} -- bin-packing sort for tasks (SMP Click)
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2004 Regents of the University of California
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

#include <click/config.h>
#include <click/standard/scheduleinfo.hh>
#include "balancedthreadsched.hh"
#include <click/task.hh>
#include <click/routerthread.hh>
#include <click/router.hh>
#include <click/master.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/task.hh>
#include <click/error.hh>

#define DEBUG 0
#define KEEP_GOOD_ASSIGNMENT 1


BalancedThreadSched::BalancedThreadSched()
    : _timer(this)
{
}

BalancedThreadSched::~BalancedThreadSched()
{
}
  
int 
BalancedThreadSched::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _interval = 1000;
    _increasing = true;
    if (cp_va_kparse(conf, this, errh, 
		     "INTERVAL", cpkP, cpUnsigned, &_interval, 
		     "INCREASING", cpkP, cpBool, &_increasing,
		     cpEnd) < 0)
	return -1;
    return 0;
}

int
BalancedThreadSched::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_msec(10);
    return 0;
}

static int task_increasing_sorter(const void *va, const void *vb) {
    Task **a = (Task **)va, **b = (Task **)vb;
    int ca = (*a)->cycles(), cb = (*b)->cycles();
    return (ca < cb ? -1 : (cb < ca ? 1 : 0));
}

static int task_decreasing_sorter(const void *va, const void *vb) {
    Task **a = (Task **)va, **b = (Task **)vb;
    int ca = (*a)->cycles(), cb = (*b)->cycles();
    return (ca < cb ? 1 : (cb < ca ? -1 : 0));
}

void
BalancedThreadSched::run_timer(Timer *)
{
    Master *m = router()->master();

    // develop load list
    Vector<int> load;
    int total_load = 0;
    for (int tid = 0; tid < m->nthreads(); tid++) {
	RouterThread *thread = m->thread(tid);
	thread->lock_tasks();
	int thread_load = 0;
	Task *end = thread->task_end();
	for (Task *t = thread->task_begin(); t != end; t = thread->task_next(t))
	    thread_load += t->cycles();
	thread->unlock_tasks();
	total_load += thread_load;
	load.push_back(thread_load);
    }
    int avg_load = total_load / m->nthreads();

    for (int rounds = 0; rounds < m->nthreads(); rounds++) {
	// find min and max loaded threads
	int min_tid = 0, max_tid = 0;
	for (int tid = 1; tid < m->nthreads(); tid++)
	    if (load[tid] < load[min_tid])
		min_tid = tid;
	    else if (load[tid] > load[max_tid])
		max_tid = tid;

#if KEEP_GOOD_ASSIGNMENT
	// do nothing if load difference is minor
	if ((avg_load - load[min_tid] < (avg_load >> 3))
	    && (load[max_tid] - avg_load < (avg_load >> 3)))
	    break;
#endif

	// lock max_thread
	RouterThread *thread = m->thread(max_tid);
	thread->lock_tasks();
	
	// collect tasks from max-loaded thread
	total_load -= load[max_tid];
	load[max_tid] = 0;
	Vector<Task *> tasks;
	Task *end = thread->task_end();
	for (Task *t = thread->task_begin(); t != end; t = thread->task_next(t)) {
	    load[max_tid] += t->cycles();
	    tasks.push_back(t);
	}
	total_load += load[max_tid];
	avg_load = total_load / m->nthreads();
	
	// sort tasks by cycle count
	click_qsort(tasks.begin(), tasks.size(), sizeof(Task *), (_increasing ? task_increasing_sorter : task_decreasing_sorter));

	// move tasks
	int highwater = avg_load + (avg_load >> 2);
	for (Task **tt = tasks.begin(); tt < tasks.end(); tt++)
	    if (load[min_tid] + (*tt)->cycles() < highwater
		&& load[min_tid] + 2*(*tt)->cycles() <= load[max_tid]) {
		load[min_tid] += (*tt)->cycles();
		load[max_tid] -= (*tt)->cycles();
		(*tt)->move_thread(min_tid);
	    }

	// done with this round!
	thread->unlock_tasks();
    }
  
    _timer.schedule_after_msec(_interval);
}

ELEMENT_REQUIRES(multithread)
EXPORT_ELEMENT(BalancedThreadSched BalancedThreadSched-SortedTaskSched)
