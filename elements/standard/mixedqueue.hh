// -*- c-basic-offset: 4 -*-
#ifndef CLICK_MIXEDQUEUE_HH
#define CLICK_MIXEDQUEUE_HH
#include "notifierqueue.hh"
CLICK_DECLS

/*
=c

MixedQueue
MixedQueue(CAPACITY)

=s storage

stores packets in a FIFO/LIFO queue

=d

Stores incoming packets in a mixed first-in-first-out/last-in-first-out queue.
In particular, MixedQueue's first input is FIFO, but its second input is LIFO.
The queue is full when it holds CAPACITY packets. When full, MixedQueue drops
incoming FIFO packets, but drops the oldest packet to make room for incoming
LIFO packets. The default for CAPACITY is 1000.

MixedQueue notifies interested parties when it becomes empty and when a
formerly-empty queue receives a packet. The empty notification takes place
some time after the queue goes empty, to prevent thrashing for queues that
hover around 1 or 2 packets long.

=e

This diagram shows the state of a MixedQueue after 5 pushes, of packets A
through E. The queue's head is on the left.

     initial state        empty
     push(0, A)           [A]
     push(0, B)           [A B]
     push(1, C)           [C A B]
     push(0, D)           [C A B D]
     push(1, E)           [E C A B D]

=head1 SYNCHRONIZATION

Note for multithreaded Click: Unlike Queue, whose input and output ports need
not be synchronized, MixedQueue requires synchronization between its LIFO
input and its output. You will need to prevent a thread from pushing to the
LIFO input at the same time that a different thread pulls from the output.

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

=a Queue, SimpleQueue, FrontDropQueue */

class MixedQueue : public NotifierQueue { public:

    MixedQueue();

    const char *class_name() const	{ return "MixedQueue"; }
    const char *port_count() const	{ return "2/1-2"; }
    void *cast(const char *);

    void push(int port, Packet *);

};

CLICK_ENDDECLS
#endif
