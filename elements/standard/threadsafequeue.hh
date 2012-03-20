// -*- c-basic-offset: 4 -*-
#ifndef CLICK_THREADSAFEQUEUE_HH
#define CLICK_THREADSAFEQUEUE_HH
#include "fullnotequeue.hh"
CLICK_DECLS

/*
=c

ThreadSafeQueue
ThreadSafeQueue(CAPACITY)

=s storage

stores packets in a FIFO queue

=d

Stores incoming packets in a first-in-first-out queue.
Drops incoming packets if the queue already holds CAPACITY packets.
The default for CAPACITY is 1000.

This variant of the default Queue is (should be) completely thread safe, in
that it supports multiple concurrent pushers and pullers.  In all respects
other than thread safety it behaves just like Queue, and like Queue it has
non-full and non-empty notifiers.

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

=a Queue, SimpleQueue, NotifierQueue, MixedQueue, FrontDropQueue */

class ThreadSafeQueue : public FullNoteQueue { public:

    ThreadSafeQueue();

    const char *class_name() const		{ return "ThreadSafeQueue"; }
    void *cast(const char *);

    int live_reconfigure(Vector<String> &conf, ErrorHandler *errh);

    void push(int port, Packet *);
    Packet *pull(int port);

  private:

    atomic_uint32_t _xhead;
    atomic_uint32_t _xtail;

};

CLICK_ENDDECLS
#endif
