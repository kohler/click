#ifndef QUEUE_HH
#define QUEUE_HH
#include "element.hh"
#include "bitvector.hh"
#include "glue.hh"

/*
 * =c
 * Queue
 * Queue(MAX)
 * =d
 * Stores incoming packets in a first-in-first-out queue.
 * Drops incoming packets if the queue already holds MAX packets.
 * The default for MAX is 1000.
 * =a RED
 */

class Storage : public Element { protected:

  int _max;
  int _head;
  int _tail;

  int _empty_jiffies;
  
 public:

  Storage()					{ }
  Storage(int i, int o)				: Element(i, o) { }

  bool is_a(const char *) const;
  
  operator bool() const				{ return _head != _tail; }
  bool empty() const				{ return _head == _tail; }
  int size() const;
  int capacity() const				{ return _max; }

  int empty_jiffies() const			{ return _empty_jiffies; }
  
};

class Queue : public Storage {
  
  Packet **_q;
  int _drops;
  int _max_length;

  int next_i(int i) const			{ return (i!=_max ? i+1 : 0); }
  
 public:
  
  Queue();
  ~Queue();
  
  const char *class_name() const		{ return "Queue"; }
  bool is_a(const char *) const;
  Processing default_processing() const		{ return PUSH_TO_PULL; }
  
  int drops() const				{ return _drops; }
  int max_length() const			{ return _max_length; }
  
  void enq(Packet *);
  Packet *deq();
  Packet *head() const;
  
  Queue *clone() const				{ return new Queue; }
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  bool can_live_reconfigure() const		{ return true; }
  int live_reconfigure(const String &, ErrorHandler *);
  void add_handlers();
  
  void push(int port, Packet *);
  Packet *pull(int port);
};


inline int
Storage::size() const
{
  return (_tail >= _head ? _tail - _head : _max - (_head - _tail) + 1);
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
