#ifndef QUEUE_HH
#define QUEUE_HH
#include "element.hh"
#include "bitvector.hh"
#include "glue.hh"

/*
 * =c
 * Queue
 * Queue(CAPACITY)
 * =s
 * stores packets in a FIFO queue
 * V<storage>
 * =d
 * Stores incoming packets in a first-in-first-out queue.
 * Drops incoming packets if the queue already holds CAPACITY packets.
 * The default for CAPACITY is 1000.
 * =h length read-only
 * Returns the current number of packets in the queue.
 * =h highwater_length read-only
 * Returns the maximum number of packets that have ever been in the queue at once.
 * =h capacity read/write
 * Returns or sets the queue's capacity.
 * =h drops read-only
 * Returns the number of packets dropped by the Queue so far.
 * =a RED, FrontDropQueue
 */

class Storage {

  int _capacity;
  int _head;
  int _tail;

  int _empty_jiffies;

  friend class Queue;
  friend class FrontDropQueue;
  
 public:

  Storage()					{ }

  operator bool() const				{ return _head != _tail; }
  bool empty() const				{ return _head == _tail; }
  int size() const;
  int capacity() const				{ return _capacity; }

  int empty_jiffies() const			{ return _empty_jiffies; }
  
};

class Queue : public Element, public Storage {
  
  Packet **_q;
  int _drops;
  int _max_length;

  int next_i(int i) const		{ return (i!=_capacity ? i+1 : 0); }
  int prev_i(int i) const		{ return (i!=0 ? i-1 : _capacity); }

  friend class FrontDropQueue;
  
 public:
  
  Queue();
  ~Queue();
  
  const char *class_name() const		{ return "Queue"; }
  void *cast(const char *);
  const char *processing() const		{ return PUSH_TO_PULL; }
  
  int drops() const				{ return _drops; }
  int max_length() const			{ return _max_length; }
  
  void enq(Packet *);
  Packet *deq();
  Packet *head() const;
  
  Queue *clone() const				{ return new Queue; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  bool can_live_reconfigure() const		{ return true; }
  int live_reconfigure(const Vector<String> &, ErrorHandler *);
  void take_state(Element *, ErrorHandler *);
  void add_handlers();
  
  void push(int port, Packet *);
  Packet *pull(int port);
  
};


inline int
Storage::size() const
{
  register int x = _tail - _head;
  return (x >= 0 ? x : _capacity + x + 1);
}

inline void
Queue::enq(Packet *p)
{
  assert(p);
  int next = next_i(_tail);
  if (next != _head) {
    _q[_tail] = p;
    _tail = next;
  } else
    p->kill();
}

inline Packet *
Queue::deq()
{
  if (_head != _tail) {
    Packet *p = _q[_head];
    assert(p);
    _head = next_i(_head);
    if (_head == _tail)
      _empty_jiffies = click_jiffies();
    return p;
  } else
    return 0;
}

inline Packet *
Queue::head() const
{
  return (_head != _tail ? _q[_head] : 0);
}

#endif
