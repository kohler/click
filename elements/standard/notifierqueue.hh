// -*- c-basic-offset: 4 -*-
#ifndef CLICK_NOTIFIERQUEUE_HH
#define CLICK_NOTIFIERQUEUE_HH
#include <click/element.hh>
#include <click/notifier.hh>
#include "simplequeue.hh"
CLICK_DECLS

/*
=c

Queue
Queue(CAPACITY)

=s storage

stores packets in a FIFO queue, with notification

=d

Stores incoming packets in a first-in-first-out queue.
Drops incoming packets if the queue already holds CAPACITY packets.
The default for CAPACITY is 1000.

Queue notifies interested parties when it becomes empty and when a
formerly-empty queue receives a packet. The empty notification takes place
some time after the queue goes empty for the first time, to prevent thrashing
for queues that tend to hover around 1 or 2 packets. In all other respects, it
behaves like SimpleQueue.

You may also use the old element name "NotifierQueue".

=h length read-only

Returns the current number of packets in the queue.

=h highwater_length read-only

Returns the maximum number of packets that have ever been in the queue at once.

=h capacity read/write

Returns or sets the queue's capacity.

=h drops read-only

Returns the number of packets dropped by the queue so far.

=h reset_counts write-only

When written, resets the C<drops> and C<highwater_length> counters.

=h reset write-only

When written, drops all packets in the queue.

=a SimpleQueue */

class NotifierQueue : public SimpleQueue, public Notifier { public:

    NotifierQueue();
    ~NotifierQueue();

    const char *class_name() const		{ return "Queue"; }
    void *cast(const char *);
    NotifierQueue *clone() const		{ return new NotifierQueue; }

    int configure(Vector<String> &conf, ErrorHandler *);
    
    void push(int port, Packet *);
    Packet *pull(int port);

  private:

    enum { SLEEPINESS_TRIGGER = 8 };
    int _sleepiness;
    
};

CLICK_ENDDECLS
#endif
