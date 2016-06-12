// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TASKTHREADTEST_HH
#define CLICK_TASKTHREADTEST_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=c

TaskThreadTest([I<keywords>])

=s test

runs Task stress test

=d

TaskThreadTest is a stress test for Click Task scheduling. It creates N
tasks (default 4096) that are always scheduled, then continually (1) deletes
and recreates FREE of the tasks (default 128) and (2) reschedules CHANGE
of the tasks onto another thread (default 1024).

TaskThreadTest does not route packets.

Keyword arguments are:

=over 8

=item N

Unsigned integer. Number of tasks. Default is 4096.

=item FREE

Unsigned integer. Number of tasks to delete and recreate on each main-task
scheduling. Default is 128.

=item CHANGE

Unsigned integer. Number of tasks to change threads on each main-task
scheduling. Default is 1024.

=item MAIN_TICKETS

Unsigned integer. Scheduling priority for the main task.

=back

=h runs r

Space-separated array of counters. How many times a task has run on each
thread.

*/

class TaskThreadTest : public Element { public:
    TaskThreadTest() CLICK_COLD;

    const char* class_name() const		{ return "TaskThreadTest"; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void cleanup(CleanupStage stage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    bool run_task(Task* t);

private:
    struct ttt_stat {
        uint64_t runs;
        uint64_t padding[15];
        ttt_stat() : runs(0) {}
    };
    Task _main_task;
    Task* _tasks;
    ttt_stat* _ttt_stats;
    unsigned _ntasks;
    unsigned _free_batch;
    unsigned _change_batch;
    unsigned _main_tickets;

    static bool main_task_callback(Task*, void*);
    enum { H_RUNS };
    static String read_handler(Element*, void*) CLICK_COLD;
};

CLICK_ENDDECLS
#endif
