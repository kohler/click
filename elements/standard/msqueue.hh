#ifndef MSQUEUE_HH
#define MSQUEUE_HH

/*
 * =c
 * MSQueue
 * MSQueue(CAPACITY)
 * =s storage
 * stores packets in a FIFO queue
 * =d
 * Stores incoming packets in a multiple producer single consumer
 * first-in-first-out queue. Enqueue operations are synchronized, dequeue
 * operations are not. Drops incoming packets if the queue already holds
 * CAPACITY packets. The default for CAPACITY is 1000.
 *
 * =h length read-only
 * Returns the current number of packets in the queue.
 * =h capacity read-only
 * Returns or sets the queue's capacity.
 * =a Queue
 */

#include <click/element.hh>
#include <click/bitvector.hh>
#include <click/glue.hh>
#include <click/sync.hh>
#include "elements/standard/queue.hh"


class MSQueue : public Element {

  int _capacity;
  u_atomic32_t _head;
  u_atomic32_t _tail;
  u_atomic32_t _drops;
  Packet **_q;
  Spinlock _lock;

  int _pad[8];
  bool _can_pull;
  int _pulls;

  int next_i(int i) const { return (i!=_capacity ? i+1 : 0); }
  int prev_i(int i) const { return (i!=0 ? i-1 : _capacity); }

  static String read_handler(Element *, void *);
  
 public:
  
  MSQueue();
  virtual ~MSQueue();
  
  const char *class_name() const		{ return "MSQueue"; }
  void *cast(const char *);
  const char *processing() const		{ return PUSH_TO_PULL; }
  
  int size() const; 
  int capacity() const                          { return _capacity; }
  u_atomic32_t drops() const			{ return _drops; }

  Packet *head() const;

  MSQueue *clone() const			{ return new MSQueue; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  void push(int port, Packet *);
  Packet *pull(int port);

#ifdef __KERNEL__
#if __i386__ && HAVE_INTEL_CPU
  static void prefetch_packet(Packet *p);
#endif
#endif
};

#ifdef __KERNEL__
#if __i386__ && HAVE_INTEL_CPU
inline void
MSQueue::prefetch_packet(Packet *p)
{
  struct sk_buff *skb = p->steal_skb();
  asm volatile("prefetcht0 %0" : : "m" (skb->data));
}
#endif
#endif

inline int
MSQueue::size() const
{ 
  register int x = _tail.value() - _head.value(); 
  return (x >= 0 ? x : _capacity + x + 1);
}

inline Packet *
MSQueue::head() const
{
  return (_head.value() != _tail.value() ? _q[_head.value()] : 0);
}

#endif
