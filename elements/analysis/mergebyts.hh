// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_MERGEBYTS_HH
#define CLICK_MERGEBYTS_HH
#include <click/element.hh>

/*
=c

MergeByTimestamp(I<KEYWORDS>)

=s

merge sorted packet streams by timestamp

=io

One output, zero or more inputs

=d

MergeByTimestamp responds to pull requests by returning the chronologically
next packet pulled from its inputs, determined by packet timestamps.

Keyword arguments are:

=over 8

=item STOP

Boolean. If true, stop the driver when there are no packets available
upstream. Default is false.

=item NULL_IS_DEAD

Boolean. If true, then ignore input ports as soon as they return null pointers
instead of packets. Default is false.

=back

=e

This example merges multiple tcpdump(1) files into a single, time-sorted
stream, and stops the driver when all the files are exhausted.

  m :: MergeByTimestamp(STOP true);
  FromDump(FILE1) -> [0] m;
  FromDump(FILE2) -> [1] m;
  FromDump(FILE3) -> [2] m;
  // ...
  m -> ...;

=a

FromDump
*/

class MergeByTimestamp : public Element { public:

    MergeByTimestamp();
    ~MergeByTimestamp();

    const char *class_name() const	{ return "MergeByTimestamp"; }
    const char *processing() const	{ return PULL; }
    MergeByTimestamp *clone() const	{ return new MergeByTimestamp; }

    void notify_ninputs(int);
    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();

    Packet *pull(int);
    
  private:

    Packet **_vec;
    bool _stop;
    bool _dead_null;
    bool _new;
    
};

#endif
