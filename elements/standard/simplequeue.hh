#ifndef CLICK_SIMPLEQUEUE_HH
#define CLICK_SIMPLEQUEUE_HH
#include <click/element.hh>
#include <click/standard/storage.hh>
CLICK_DECLS

/*
=c

SimpleQueue
SimpleQueue(CAPACITY)

=s storage

stores packets in a FIFO queue

=d

Stores incoming packets in a first-in-first-out queue.
Drops incoming packets if the queue already holds CAPACITY packets.
The default for CAPACITY is 1000.

=n

The Queue element acts like SimpleQueue, but additionally notifies
interested parties when it changes state (from nonempty to empty or vice
versa).

=h length read-only

Returns the current number of packets in the queue.

=h highwater_length read-only

Returns the maximum number of packets that have ever been in the queue at once.

=h capacity read/write

Returns or sets the queue's capacity.

=h drops read-only

Returns the number of packets dropped by the Queue so far.

=h reset_counts write-only

When written, resets the C<drops> and C<highwater_length> counters.

=h reset write-only

When written, drops all packets in the Queue.

=a Queue, MixedQueue, RED, FrontDropQueue */

class SimpleQueue : public Element, public Storage { public:

  SimpleQueue();
  ~SimpleQueue();
  
  int drops() const				{ return _drops; }
  int highwater_length() const			{ return _highwater_length; }
  
  void enq(Packet *);
  void lifo_enq(Packet *);
  Packet *deq();
  Packet *head() const;
  
  const char *class_name() const		{ return "SimpleQueue"; }
  const char *processing() const		{ return PUSH_TO_PULL; }
  void *cast(const char *);
  SimpleQueue *clone() const			{ return new SimpleQueue; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  bool can_live_reconfigure() const		{ return true; }
  int live_reconfigure(Vector<String> &, ErrorHandler *);
  void take_state(Element *, ErrorHandler *);
  void add_handlers();
  
  void push(int port, Packet *);
  Packet *pull(int port);
  
 private:
  
  Packet **_q;
  int _drops;
  int _highwater_length;

  friend class FrontDropQueue;
  friend class NotifierQueue;
  friend class MixedQueue;

  static String read_handler(Element *, void *);
  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  
};


inline void
SimpleQueue::enq(Packet *p)
{
  assert(p);
  int next = next_i(_tail);
  if (next != _head) {
    _q[_tail] = p;
    _tail = next;
  } else
    p->kill();
}

inline void
SimpleQueue::lifo_enq(Packet *p)
{
  // XXX NB: significantly more dangerous in a multithreaded environment
  // than plain (FIFO) enq().
  assert(p);
  int prev = prev_i(_head);
  if (prev == _tail) {
    _tail = prev_i(_tail);
    _q[_tail]->kill();
  }
  _q[prev] = p;
  _head = prev;
}

inline Packet *
SimpleQueue::deq()
{
  if (_head != _tail) {
    Packet *p = _q[_head];
    assert(p);
    _head = next_i(_head);
    return p;
  } else
    return 0;
}

inline Packet *
SimpleQueue::head() const
{
  return (_head != _tail ? _q[_head] : 0);
}

CLICK_ENDDECLS
#endif
