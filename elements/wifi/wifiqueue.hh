#ifndef CLICK_WIFIQUEUE_HH
#define CLICK_WIFIQUEUE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/standard/storage.hh>
#include <click/packet.hh>
CLICK_DECLS

/*
=c

WifiQueue
WifiQueue(CAPACITY)

=s storage

stores packets in a FIFO queue

=d

Stores incoming packets in a first-in-first-out queue.
Drops incoming packets if the queue already holds CAPACITY packets.
The default for CAPACITY is 1000.

=n

The Queue element acts like WifiQueue, but additionally notifies
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


class SafeQueue: public Storage {
protected:
  Packet **_q;
public:
  SafeQueue(int capacity) {  
    _capacity = capacity;
    _q = new Packet *[_capacity + 1];
    if (_q == 0) {
      click_chatter("couldn't allocate in SafeQueue\n");
    }
  }
  
  ~SafeQueue() {
    if (_q) {
      delete[] _q;
    }
    _q = 0;
  }
  
  
  inline void enq(Packet *p)
  {
    assert(p);
    int next = next_i(_tail);
    if (next != _head) {
      _q[_tail] = p;
      _tail = next;
    } else {
      p->kill();
    }
  }
  
  inline Packet *deq()
  {
    assert(_q);
    if (_head != _tail) {
      Packet *p = _q[_head];
      assert(p);
      _head = next_i(_head);
      return p;
    } else {
      return 0;
    }
  }
  
  inline Packet *head() const
  {
    return (_head != _tail ? _q[_head] : 0);
  }
  
};



class WifiQueue : public Element, public Storage { public:

  WifiQueue();
  ~WifiQueue();
  
  const char *class_name() const		{ return "WifiQueue"; }
  const char *processing() const		{ return PUSH_TO_PULL; }
  void *cast(const char *);
  WifiQueue *clone() const			{ return new WifiQueue; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  
  void push(int port, Packet *);
  Packet *pull(int port);


  void add_handlers();

  static String static_print_stats(Element *e, void *);
  String print_stats();  
 private:


  class Neighbor {
  public:
    EtherAddress _eth;
    SafeQueue _q;
    SafeQueue _retry_q;
    int metric;

    Neighbor(EtherAddress eth) : _q(100),  _retry_q(5){ 
      _eth = eth;
      metric = 0;
    }
    ~Neighbor() { }
  };
  EtherAddress _bcast;
  int _capacity;
  int current_q;


  typedef BigHashMap<EtherAddress, Neighbor *> NTable;
  typedef NTable::const_iterator NTIter;
  class NTable _neighbors_m;
  Vector<EtherAddress> _neighbors_v;
  //SafeQueue _q;

  void enq(Packet *);
  void feedback(Packet *);
  Packet *next();
  Neighbor *find_neighbor(EtherAddress e);
  void wifiqueue_assert_(const char *, int, const char *) const;

};


CLICK_ENDDECLS
#endif
