// -*- c-basic-offset: 4 -*-
#ifndef CLICK_NOTIFIERQUEUE_HH
#define CLICK_NOTIFIERQUEUE_HH
#include <click/element.hh>
#include <click/notifier.hh>
#include "simplequeue.hh"
#if NOTIFIERQUEUE_LOCK
# include <click/sync.hh>
#endif
CLICK_DECLS

/*
=c

NotifierQueue
NotifierQueue(CAPACITY)

=s storage

stores packets in a FIFO queue

=d

Stores incoming packets in a first-in-first-out queue.
Drops incoming packets if the queue already holds CAPACITY packets.
The default for CAPACITY is 1000.

NotifierQueue notifies interested parties when it becomes empty and when a
formerly-empty queue receives a packet. The empty notification takes place
some time after the queue goes empty, to prevent thrashing for queues that
hover around 1 or 2 packets long. In all other respects, NotifierQueue behaves
like SimpleQueue.

=n

The Queue element acts like NotifierQueue, but additionally notifies
interested parties when it changes state from nonfull to full or vice versa.

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

=a Queue, SimpleQueue, MixedQueue, FrontDropQueue */

class NotifierQueue : public SimpleQueue, public ActiveNotifier { public:

    NotifierQueue();
    ~NotifierQueue();

    const char *class_name() const		{ return "NotifierQueue"; }
    void *cast(const char *);

    int configure(Vector<String> &conf, ErrorHandler *);
    
    void push(int port, Packet *);
    Packet *pull(int port);

#if NOTIFIERQUEUE_DEBUG
    void add_handlers();
#endif
 
  protected:

    enum { SLEEPINESS_TRIGGER = 9 };
    int _sleepiness;
#if NOTIFIERQUEUE_LOCK
    Spinlock _lock;
#endif

    friend class MixedQueue;
    friend class InOrderQueue;
    friend class ECNQueue;
    friend class TokenQueue;
#if NOTIFIERQUEUE_DEBUG
    static String read_handler(Element *, void *);
#endif
    
};

CLICK_ENDDECLS
#endif
