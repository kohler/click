// -*- c-basic-offset: 4 -*-
/*
 * taskthreadtest.{cc,hh} -- regression test element for Task
 * Eddie Kohler
 *
 * Copyright (c) 2016 Eddie Kohler
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
#include "taskthreadtest.hh"
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/master.hh>
CLICK_DECLS

TaskThreadTest::TaskThreadTest()
    : _main_task(main_task_callback, this), _tasks(), _ttt_stats(),
      _ntasks(4096), _free_batch(128), _change_batch(1024),
      _main_tickets(Task::DEFAULT_TICKETS), _main_runs(0),
      _free_cycles(0), _create_cycles(0), _change_cycles(0),
      _progress(false)
{
}

bool TaskThreadTest::main_task_callback(Task* t, void* callback) {
    TaskThreadTest* e = static_cast<TaskThreadTest*>(callback);
    // free + recreate _free_batch
    click_cycles_t cycles0 = click_get_cycles();
    unsigned n = click_random() % e->_ntasks;
    for (unsigned i = 0; i < e->_free_batch; ++i)
        e->_tasks[(n + i) % e->_ntasks].~Task();
    click_cycles_t cycles1 = click_get_cycles();
    e->_free_cycles += cycles1 - cycles0;
    cycles0 = cycles1;

    unsigned nthreads = e->master()->nthreads();
    e->router()->set_home_thread_id(e, click_random() % nthreads);
    for (unsigned i = 0; i < e->_free_batch; ++i) {
        unsigned j = (n + i) % e->_ntasks;
        new(reinterpret_cast<char*>(&e->_tasks[j])) Task(e);
        e->_tasks[j].initialize(e, true);
    }
    cycles1 = click_get_cycles();
    e->_create_cycles += cycles1 - cycles0;
    cycles0 = cycles1;

    // change thread for _change_batch
    for (unsigned i = 0; i < e->_change_batch; ++i) {
        unsigned j = click_random() % e->_ntasks;
        e->_tasks[j].move_thread((e->_tasks[j].home_thread_id() + 1) % nthreads);
    }
    cycles1 = click_get_cycles();
    e->_change_cycles += cycles1 - cycles0;

    t->fast_reschedule();
    ++e->_main_runs;
    if (e->_progress && e->_main_runs % (1 << 0) == 0) {
        click_cycles_t t = e->_free_cycles + e->_create_cycles + e->_change_cycles;
        click_chatter("%{element}: %llu runs (%llu free, %llu create, %llu change)", e, e->_main_runs,
                      (e->_free_cycles * 1000) / t,
                      (e->_create_cycles * 1000) / t,
                      (e->_change_cycles * 1000) / t);
    }
    return true;
}

int
TaskThreadTest::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	.read("N", _ntasks)
	.read("FREE", _free_batch)
	.read("CHANGE", _change_batch)
#if HAVE_STRIDE_SCHED
        .read("MAIN_TICKETS", _main_tickets)
#endif
        .read("PROGRESS", _progress)
        .complete() < 0)
        return -1;
    return 0;
}

int
TaskThreadTest::initialize(ErrorHandler*)
{
    _tasks = reinterpret_cast<Task*>(new char[sizeof(Task) * _ntasks]);
    for (unsigned i = 0; i < _ntasks; ++i) {
        new(reinterpret_cast<char*>(&_tasks[i])) Task(this);
        _tasks[i].initialize(this, true);
    }
    _ttt_stats = new ttt_stat[master()->nthreads()];
    _main_task.initialize(this, true);
#if HAVE_STRIDE_SCHED
    _main_task.set_tickets(_main_tickets);
#else
    (void) _main_tickets;
#endif
    return 0;
}

void
TaskThreadTest::cleanup(CleanupStage)
{
    if (_tasks) {
        for (unsigned i = 0; i < _ntasks; ++i)
            _tasks[i].~Task();
        delete[] reinterpret_cast<char*>(_tasks);
        _tasks = 0;
    }
    delete[] _ttt_stats;
}

bool
TaskThreadTest::run_task(Task* t)
{
    _ttt_stats[t->thread()->thread_id()].runs += 1;
    t->fast_reschedule();
    return true;
}

String
TaskThreadTest::read_handler(Element* e, void*)
{
    TaskThreadTest* t = static_cast<TaskThreadTest*>(e);
    StringAccum sa;
    for (int i = 0; i < t->master()->nthreads(); ++i)
        sa << (i ? " " : "") << t->_ttt_stats[i].runs;
    sa << "\n";
    return sa.take_string();
}

void
TaskThreadTest::add_handlers()
{
    add_read_handler("runs", read_handler, H_RUNS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TaskThreadTest)
