// -*- c-basic-offset: 4 -*-
/*
 * sortedsched.{cc,hh} -- bin-packing sort for tasks (SMP Click)
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
#include "sortedsched.hh"
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
    if (cp_va_parse(conf, this, errh, 
		    cpOptional,
		    cpUnsigned, "interval", &_interval, 
		    cpBool, "increasing?", &_increasing, cpEnd) < 0)
	return -1;
    return 0;
}

int
BalancedThreadSched::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_ms(10);
    return 0;
}

static int task_increasing_sorter(const void *va, const void *vb) {
    Task **a = (Task **)va, **b = (Task **)vb;
    return (*a)->cycles() - (*b)->cycles();
}

static int task_decreasing_sorter(const void *va, const void *vb) {
    Task **a = (Task **)va, **b = (Task **)vb;
    return (*b)->cycles() - (*a)->cycles();
}

void
BalancedThreadSched::run_timer()
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
		(*tt)->change_thread(min_tid);
	    }

	// done with this round!
	thread->unlock_tasks();
    }
  
    _timer.schedule_after_ms(_interval);
}

#if 0
void
BalancedThreadSched::run_timer()
{
    Vector<Task*> tasks;
    unsigned total_load = 0;
    unsigned avg_load;
    int n = router()->nthreads();
    int load[n];
  
    for(int i=0; i<n; i++) load[i] = 0;

    TaskList *task_list = router()->task_list();
    task_list->lock();
    Task *t = task_list->all_tasks_next();
    while (t != task_list) {
	total_load += t->cycles();
	if (t->thread_preference() >= 0) 
	    load[t->thread_preference()] += t->cycles();
	tasks.push_back(t);
	t = t->all_tasks_next();
    }
    task_list->unlock();
    avg_load = total_load / n;

#if KEEP_GOOD_ASSIGNMENT
    int ii;
    for(ii=0; ii<n; ii++) {
	unsigned diff = avg_load>load[ii] ? avg_load-load[ii] : load[ii]-avg_load;
	if (diff > (avg_load>>3)) {
#if DEBUG > 1
	    click_chatter("load balance, avg %u, diff %u", avg_load, diff);
#endif
	    break;
	}
    }
    if (ii == n) {
	_timer.schedule_after_ms(_interval);
	return;
    }
#endif
  
#if DEBUG > 1
    int print = 0;
    int high = 0;
#endif

    // slow sorting algorithm, but works okay for small number of tasks
    Vector<Task*> sorted;
    for (int i=0; i<tasks.size(); i++) {
	int max = 0;
	int which = -1;
	for (int j=0; j<tasks.size(); j++) {
	    if (tasks[j] && tasks[j]->cycles() > max) { 
		which = j; 
		max = tasks[j]->cycles();
	    }
	}
	assert(which >= 0);
	sorted.push_back(tasks[which]);
	tasks[which] = 0;
#if DEBUG > 1
	if (i == 0) high = max;
#endif
    }

#if DEBUG > 1
    if (high >= 500) {
	print = 1;
	unsigned now = click_jiffies();
	for(int i=0; i<sorted.size(); i++) {
	    Element *e = sorted[i]->element();
	    if (e)
		click_chatter("%u: %s %d, was on %d", 
			      now, e->id().c_str(), 
			      sorted[i]->cycles(), 
			      sorted[i]->thread_preference());
	}
    }
#endif

    Vector<Task*> schedule[n];
    for(int i=0; i<n; i++) load[i] = 0;
    int min, which;
    int i = _increasing ? sorted.size()-1 : 0;
    while (1) {
	min = load[0];
	which = 0;
	for (int j = 1; j < n; j++) {
	    if (load[j] < min) {
		which = j;
		min = load[j];
	    }
	}
	load[which] += sorted[i]->cycles();
	schedule[which].push_back(sorted[i]);
	sorted[i]->change_thread(which);
	if (_increasing) {
	    if (i == 0) break;
	    else i--;
	} else {
	    if (i == sorted.size()-1) break;
	    else i++;
	}
    }
  
#if DEBUG > 1
    if (print) {
	unsigned now = click_jiffies();
	for(int i=0; i<sorted.size(); i++) {
	    Element *e = sorted[i]->element();
	    if (e) 
		click_chatter("%u: %s %d, now on %d (%d)", 
			      now, e->id().c_str(), 
			      sorted[i]->cycles(), 
			      sorted[i]->thread_preference(), avg_load);
	}
	print = 0;
	click_chatter("\n");
    }
#endif

    _timer.schedule_after_ms(_interval);
}
#endif

ELEMENT_REQUIRES(linuxmodule smpclick)
EXPORT_ELEMENT(BalancedThreadSched BalancedThreadSched-SortedTaskSched)
