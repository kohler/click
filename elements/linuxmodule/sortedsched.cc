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
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/task.hh>
#include <click/router.hh>
#include <click/error.hh>

#define DEBUG 0
#define KEEP_GOOD_ASSIGNMENT 1


SortedTaskSched::SortedTaskSched()
    : _timer(this)
{
    MOD_INC_USE_COUNT;
}

SortedTaskSched::~SortedTaskSched()
{
    MOD_DEC_USE_COUNT;
}
  
int 
SortedTaskSched::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _interval = 1000;
    _increasing = true;
    if (cp_va_parse(conf, this, errh, 
		    cpOptional,
		    cpUnsigned, "interval", &_interval, 
		    cpBool, "increasing?", &_increasing, 0) < 0)
	return -1;
    return 0;
}

int
SortedTaskSched::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_ms(2000);
    return 0;
}

void
SortedTaskSched::run_timer()
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
			      now, e->id().cc(), 
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
			      now, e->id().cc(), 
			      sorted[i]->cycles(), 
			      sorted[i]->thread_preference(), avg_load);
	}
	print = 0;
	click_chatter("\n");
    }
#endif

    _timer.schedule_after_ms(_interval);
}

ELEMENT_REQUIRES(false linuxmodule smpclick)
EXPORT_ELEMENT(SortedTaskSched)
