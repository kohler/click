#ifndef CLICK_TIMEDUNQUEUE_HH
#define CLICK_TIMEDUNQUEUE_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * TimedUnqueue(INTERVAL [, BURST])
 * =s shaping
 * pull-to-push converter
 * =d
 *
 * Pulls at most BURST packets per INTERVAL (seconds) from its input, pushing
 * them out its single output.  Default BURST is 1.
 *
 * If BURST is 1, and packets arrive upstream at a rate of less than 1 packet
 * per INTERVAL, then in steady state TimedUnqueue will impose relatively
 * little delay on the packet stream.  In other situations, TimedUnqueue may
 * impose an average delay of INTERVAL/2 seconds per packet, even for low
 * input rates.  This is because it checks for packets at most once every
 * INTERVAL seconds.
 *
 * There is usually a Queue upstream of each TimedUnqueue element.
 *
 * =n
 * The UNIX and Linux timers have granularity of about 10
 * milliseconds, so this TimedUnqueue can only produce high packet
 * rates by being bursty.
 *
 * =a RatedUnqueue, Unqueue, Burster
 */

class TimedUnqueue : public Element { public:

    TimedUnqueue() CLICK_COLD;

    const char *class_name() const		{ return "TimedUnqueue"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return PULL_TO_PUSH; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;

    bool run_task(Task *task);

  protected:

    int _burst;
    Task _task;
    Timer _timer;
    unsigned _interval;
    enum { use_signal = 1 };
    NotifierSignal _signal;

};

CLICK_ENDDECLS
#endif
