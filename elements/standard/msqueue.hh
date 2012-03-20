#ifndef CLICK_MSQUEUE_HH
#define CLICK_MSQUEUE_HH
#include "threadsafequeue.hh"
CLICK_DECLS

/*
 * =c
 * MSQueue
 * MSQueue(CAPACITY)
 * =deprecated ThreadSafeQueue
 * =s threads
 * stores packets in a FIFO queue
 * =d
 * Stores incoming packets in a multiple producer single consumer
 * first-in-first-out queue. Enqueue operations are synchronized, dequeue
 * operations are not. Drops incoming packets if the queue already holds
 * CAPACITY packets. The default for CAPACITY is 1000.
 *
 * =h length read-only
 * Returns the current number of packets in the queue.
 * =h dropd read-only
 * Returns the number of packets dropped by the queue so far.
 * =h capacity read/write
 * Returns or sets the queue's capacity.
 * =a Queue
 */

class MSQueue : public ThreadSafeQueue { public:

    MSQueue();

    const char *class_name() const		{ return "MSQueue"; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *);

#if CLICK_LINUXMODULE && __i386__ && HAVE_INTEL_CPU
    static void prefetch_packet(Packet *p);
#endif

};

#if CLICK_LINUXMODULE && __i386__ && HAVE_INTEL_CPU
inline void
MSQueue::prefetch_packet(Packet *p)
{
  struct sk_buff *skb = p->skb();
  asm volatile("prefetcht0 %0" : : "m" (skb->data));
}
#endif

CLICK_ENDDECLS
#endif
