// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TIMESORTEDSCHED_HH
#define CLICK_TIMESORTEDSCHED_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

TimeSortedSched(I<KEYWORDS>)

=s analysis

merge sorted packet streams by timestamp

=io

One output, zero or more inputs

=d

TimeSortedSched responds to pull requests by returning the chronologically
next packet pulled from its inputs, determined by packet timestamps.

TimeSortedSched listens for notification from its inputs to avoid useless
pulls, and provides notification for its output.

Keyword arguments are:

=over 8

=item STOP

Boolean. If true, stop the driver when there are no packets available (and the
upstream notifiers indicate that no packets will become available soon).
Default is false.

=back

=n

TimeSortedSched is a notifier signal, active iff any of the upstream notifiers
are active.

=e

This example merges multiple tcpdump(1) files into a single, time-sorted
stream, and stops the driver when all the files are exhausted.

  tss :: TimeSortedSched(STOP true);
  FromDump(FILE1) -> [0] tss;
  FromDump(FILE2) -> [1] tss;
  FromDump(FILE3) -> [2] tss;
  // ...
  tss -> ...;

=a

FromDump
*/

class TimeSortedSched : public Element, public PassiveNotifier { public:
    // NB: Notifier cannot be Active, or we would have rescheduling conflicts.
    // Example:
    // 1. We are unscheduled and off.
    // 2. Upstream Notifier wakes up, reschedules downstream puller.
    // 3. Downstream puller Task runs, calls our pull() function.
    // 4. We wake up, call wake_notifiers().
    // 5. That eventually calls downstream puller Task's fast_reschedule()!!
    // 6. We return to downstream puller's run_task().
    // 7. Downstream puller's run_task() calls fast_reschedule()!! Crash.
    // Principle: Do not call ActiveNotifier::wake_listeners() on a call
    // from downstream listeners.

    TimeSortedSched();
    ~TimeSortedSched();

    const char *class_name() const	{ return "TimeSortedSched"; }
    const char *processing() const	{ return PULL; }
    void *cast(const char *);

    void notify_ninputs(int);
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    Packet *pull(int);

    // from Notifier
    SearchOp notifier_search_op();
    
  private:

    Packet **_vec;
    NotifierSignal *_signals;
    bool _stop;
    
};

CLICK_ENDDECLS
#endif
