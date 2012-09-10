// -*- c-basic-offset: 4 -*-
#ifndef CLICK_QUICKNOTEQUEUE_HH
#define CLICK_QUICKNOTEQUEUE_HH
#include "fullnotequeue.hh"
CLICK_DECLS

/*
=c

QuickNoteQueue
QuickNoteQueue(CAPACITY)

=s storage

stores packets in a FIFO queue

=d

Stores incoming packets in a first-in-first-out queue.
Drops incoming packets if the queue already holds CAPACITY packets.
The default for CAPACITY is 1000.

QuickNoteQueue notifies interested parties when it becomes empty and when a
formerly-empty queue receives a packet.  Unlike the Queue element,
QuickNoteQueue's empty notification takes place I<immediately> after the queue
goes empty.  Like Queue, QuickNoteQueue additionally notifies interested
parties that it is non-full, and when a formerly-full queue gains some free
space.  In all respects but notification, Queue behaves exactly like
SimpleQueue.

B<Multithreaded Click note:> QuickNoteQueue is designed to be used in an
environment with at most one concurrent pusher and at most one concurrent
puller.  Thus, at most one thread pushes to the QuickNoteQueue at a time and
at most one thread pulls from the QuickNoteQueue at a time.  Different threads
can push to and pull from the QuickNoteQueue concurrently, however.  See
ThreadSafeQueue for a queue that can support multiple concurrent pushers and
pullers.

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

=a Queue, ThreadSafeQueue, SimpleQueue, NotifierQueue, MixedQueue,
FrontDropQueue */

class QuickNoteQueue : public FullNoteQueue { public:

    QuickNoteQueue() CLICK_COLD;

    const char *class_name() const		{ return "QuickNoteQueue"; }
    void *cast(const char *name);

    // FullNoteQueue's configure() suffices

    // FullNoteQueue's push() suffices
    Packet *pull(int port);

};

CLICK_ENDDECLS
#endif
