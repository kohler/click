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
pulls, and provides notification for its outputs.

Keyword arguments are:

=over 8

=item STOP

Boolean. If true, stop the driver when there are no packets available
upstream. Default is false.

=back

=e

This example merges multiple tcpdump(1) files into a single, time-sorted
stream, and stops the driver when all the files are exhausted.

  m :: TimeSortedSched(STOP true);
  FromDump(FILE1) -> [0] m;
  FromDump(FILE2) -> [1] m;
  FromDump(FILE3) -> [2] m;
  // ...
  m -> ...;

=a

FromDump
*/

class TimeSortedSched : public Element, public Notifier { public:

    TimeSortedSched();
    ~TimeSortedSched();

    const char *class_name() const	{ return "TimeSortedSched"; }
    const char *processing() const	{ return PULL; }
    void *cast(const char *);
    TimeSortedSched *clone() const	{ return new TimeSortedSched; }

    void notify_ninputs(int);
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    Packet *pull(int);
    
  private:

    Packet **_vec;
    NotifierSignal *_signals;
    bool _stop;
    
};

CLICK_ENDDECLS
#endif
