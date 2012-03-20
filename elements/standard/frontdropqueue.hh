#ifndef CLICK_FRONTDROPQUEUE_HH
#define CLICK_FRONTDROPQUEUE_HH
#include "notifierqueue.hh"
CLICK_DECLS

/*
=c

FrontDropQueue
FrontDropQueue(CAPACITY)

=s storage

stores packets in drop-from-front FIFO queue

=d

Stores incoming packets in a first-in-first-out queue. Drops the head packet
before inserting the incoming packet if the queue already holds CAPACITY
packets. The default for CAPACITY is 1000.

=h length read-only

Returns the current number of packets in the queue.

=h highwater_length read-only

Returns the maximum number of packets that have ever been in the queue at once.

=h capacity read/write

Returns or sets the queue's capacity.

=h drops read-only

Returns the number of packets dropped by the Queue so far.  Dropped packets
are emitted on output 1 if output 1 exists.

=h reset_counts write-only

When written, resets the C<drops> and C<highwater_length> counters.

=h reset write-only

When written, drops all packets in the Queue.

=a Queue, SimpleQueue, MixedQueue, RED
*/

class FrontDropQueue : public NotifierQueue { public:

  FrontDropQueue();

  const char *class_name() const		{ return "FrontDropQueue"; }
  void *cast(const char *);

  int live_reconfigure(Vector<String> &, ErrorHandler *);
  void take_state(Element *, ErrorHandler *);

  void push(int port, Packet *);

};

CLICK_ENDDECLS
#endif
