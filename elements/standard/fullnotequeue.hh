// -*- c-basic-offset: 4 -*-
#ifndef CLICK_FULLNOTEQUEUE_HH
#define CLICK_FULLNOTEQUEUE_HH
#include "notifierqueue.hh"
CLICK_DECLS

/*
=c

Queue
Queue(CAPACITY)

=s storage

stores packets in a FIFO queue

=d

Stores incoming packets in a first-in-first-out queue.
Drops incoming packets if the queue already holds CAPACITY packets.
The default for CAPACITY is 1000.

Queue notifies interested parties when it becomes empty and when a
formerly-empty queue receives a packet.  The empty notification takes place
some time after the queue goes empty, to prevent thrashing for queues that
hover around 1 or 2 packets long.  This behavior is the same as that of
NotifierQueue.  (See QuickNoteQueue for an alternative.)  Queue additionally
notifies interested parties that it is non-full, and when a formerly-full
queue gains some free space.  In all respects but notification, Queue behaves
exactly like SimpleQueue.

You may also use the old element name "FullNoteQueue".

B<Multithreaded Click note:> Queue is designed to be used in an environment
with at most one concurrent pusher and at most one concurrent puller.  Thus,
at most one thread pushes to the Queue at a time and at most one thread pulls
from the Queue at a time.  Different threads can push to and pull from the
Queue concurrently, however.  See ThreadSafeQueue for a queue that can support
multiple concurrent pushers and pullers.

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

=a ThreadSafeQueue, QuickNoteQueue, SimpleQueue, NotifierQueue, MixedQueue,
FrontDropQueue */

class FullNoteQueue : public NotifierQueue { public:

    FullNoteQueue() CLICK_COLD;

    const char *class_name() const		{ return "Queue"; }
    void *cast(const char *);

    int configure(Vector<String> &conf, ErrorHandler *) CLICK_COLD;
    int live_reconfigure(Vector<String> &conf, ErrorHandler *errh);
#if CLICK_DEBUG_SCHEDULING
    void add_handlers() CLICK_COLD;
#endif

    void push(int port, Packet *p);
    Packet *pull(int port);

  protected:

    ActiveNotifier _full_note;

    inline void push_success(Storage::index_type h, Storage::index_type t,
			     Storage::index_type nt, Packet *p);
    inline void push_failure(Packet *p);
    inline Packet *pull_success(Storage::index_type h,
				Storage::index_type nh);
    inline Packet *pull_failure();

#if CLICK_DEBUG_SCHEDULING
    static String read_handler(Element *e, void *user_data) CLICK_COLD;
#endif

};

inline void
FullNoteQueue::push_success(Storage::index_type h, Storage::index_type t,
			    Storage::index_type nt, Packet *p)
{
    _q[t] = p;
    packet_memory_barrier(_q[t], _tail);
    _tail = nt;

    int s = size(h, nt);
    if (s > _highwater_length)
	_highwater_length = s;

    _empty_note.wake();

    if (s == capacity()) {
	_full_note.sleep();
#if HAVE_MULTITHREAD
	// Work around race condition between push() and pull().
	// We might have just undone pull()'s Notifier::wake() call.
	// Easiest lock-free solution: check whether we should wake again!
	if (size() < capacity())
	    _full_note.wake();
#endif
    }
}

inline void
FullNoteQueue::push_failure(Packet *p)
{
    if (_drops == 0 && _capacity > 0)
	click_chatter("%p{element}: overflow", this);
    _drops++;
    checked_output_push(1, p);
}

inline Packet *
FullNoteQueue::pull_success(Storage::index_type h,
			    Storage::index_type nh)
{
    Packet *p = _q[h];
    packet_memory_barrier(_q[h], _head);
    _head = nh;

    _sleepiness = 0;
    _full_note.wake();
    return p;
}

inline Packet *
FullNoteQueue::pull_failure()
{
    if (_sleepiness >= SLEEPINESS_TRIGGER) {
        _empty_note.sleep();
#if HAVE_MULTITHREAD
	// Work around race condition between push() and pull().
	// We might have just undone push()'s Notifier::wake() call.
	// Easiest lock-free solution: check whether we should wake again!
	if (size())
	    _empty_note.wake();
#endif
    } else
	++_sleepiness;
    return 0;
}

CLICK_ENDDECLS
#endif
