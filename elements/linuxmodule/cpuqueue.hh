#ifndef CPUQUEUE_HH
#define CPUQUEUE_HH

/*
 * =c
 * CPUQueue
 * CPUQueue(CAPACITY)
 * =s threads
 * stores packets in FIFO queues.
 * =d
 *
 * Stores incoming packets in a first-in-first-out queue. Each CPU has its own
 * queue. The incoming packet is always enqueued onto the queue of the CPU
 * calling the push method. Drops incoming packets if the queue already holds
 * CAPACITY packets. The default for CAPACITY is 128.
 *
 * =a Queue
 */

#include <click/element.hh>
#if NR_CPUS > 256
# error "too many CPUs for CPUQueue"
#endif

class CPUQueue : public Element {
  struct {
    Packet **_q;
    unsigned _head;
    unsigned _tail;
    unsigned _pad[5];
  } _q[NR_CPUS];

  unsigned _last;
  unsigned _capacity;
  unsigned _drops;

  int next_i(int i) const { return (i!=_capacity ? i+1 : 0); }
  int prev_i(int i) const { return (i!=0 ? i-1 : _capacity); }
  Packet *deq(int);

  static String read_handler(Element *, void *);

 public:

  CPUQueue();
  ~CPUQueue();

  const char *class_name() const		{ return "CPUQueue"; }
  const char *port_count() const		{ return "1/1-"; }
  const char *processing() const		{ return PUSH_TO_PULL; }
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);

  unsigned drops() const			{ return _drops; }
  unsigned capacity() const			{ return _capacity; }

  int configure(Vector<String> &, ErrorHandler *);

  void push(int port, Packet *);
  Packet *pull(int port);

  void add_handlers();
};

#endif
