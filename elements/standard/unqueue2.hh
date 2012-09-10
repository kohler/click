#ifndef CLICK_UNQUEUE2_HH
#define CLICK_UNQUEUE2_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * Unqueue2([BURST, I<keywords> QUEUES])
 * =s shaping
 * pull-to-push converter
 * =deprecated Unqueue
 * =d
 * Pulls packets whenever they are available, then pushes them out its single
 * output. Pulls a maximum of BURSTSIZE packets every time it is scheduled,
 * unless downstream queues are full. Default BURSTSIZE is 1. If BURSTSIZE is
 * 0, pull until nothing comes back. Unqueue2 will not pull if there is a
 * downstream queue that is full. It will also limit burst size to equal to
 * the number of available slots in the fullest downstream queue.
 *
 * The Unqueue element should be preferred for most purposes.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item QUEUES
 *
 * The Storage elements whose sizes should be checked.  If empty, then don't
 * check downstream Storage elements' sizes.  Defaults to the set of downstream
 * Storage elements.
 *
 * =item QUIET
 *
 * Boolean. If true, do not print a deprecation warning on configuration.
 *
 * =back
 *
 * =a Unqueue, RatedUnqueue, BandwidthRatedUnqueue
 */

class Unqueue2 : public Element { public:

    Unqueue2();

    const char *class_name() const		{ return "Unqueue2"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return PULL_TO_PUSH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    bool run_task(Task *);

    static String read_param(Element *e, void *) CLICK_COLD;

  private:

    int _burst;
    unsigned _count;
    Task _task;
    Vector<Storage *> _queues;
    bool _explicit_queues;
    NotifierSignal _signal;

};

CLICK_ENDDECLS
#endif
