// -*- c-basic-offset: 4 -*-
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
    int highwater_length() const		{ return _highwater_length; }
  
    void enq(Packet *);
    void lifo_enq(Packet *);
    Packet *deq();
    Packet *head() const;

    template <typename Filter> Packet *yank1(Filter);
    template <typename Filter> Packet *yank1_peek(Filter);
    template <typename Filter> int yank(Filter, Vector<Packet *> &);

    const char *class_name() const		{ return "SimpleQueue"; }
    const char *processing() const		{ return PUSH_TO_PULL; }
    void *cast(const char *);
  
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
    friend class TokenQueue;
    friend class InOrderQueue;
    friend class ECNQueue;

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

template <typename Filter>
Packet *
SimpleQueue::yank1(Filter filter)
    /* Remove from the queue and return the first packet that matches
       'filter(Packet *)'. The returned packet must be deallocated by the
       caller. */
{
    for (int trav = _head; trav != _tail; trav = next_i(trav))
	if (filter(_q[trav])) {
	    Packet *p = _q[trav];
	    int prev = prev_i(trav);
	    while (trav != _head) {
		_q[trav] = _q[prev];
		trav = prev;
		prev = prev_i(prev);
	    }
	    _head = next_i(_head);
	    return p;
	}
    return 0;
}

template <typename Filter>
Packet *
SimpleQueue::yank1_peek(Filter filter)
    /* return the first packet that matches
       'filter(Packet *)'. The returned packet must *NOT* be deallocated by the
       caller. */
{
    for (int trav = _head; trav != _tail; trav = next_i(trav))
	if (filter(_q[trav])) {
	    Packet *p = _q[trav];
	    return p;
	}
    return 0;
}

template <typename Filter>
int
SimpleQueue::yank(Filter filter, Vector<Packet *> &yank_vec)
    /* Removes from the queue and adds to 'yank_vec' all packets in the queue
       that match 'filter(Packet *)'. Packets are added to 'yank_vec' in LIFO
       order, so 'yank_vec.back()' will equal the first packet in the queue
       that matched 'filter()'. Caller should deallocate any packets returned
       in 'yank_vec'. Returns the number of packets yanked. */
{
    int write_ptr = _tail;
    int nyanked = 0;
    for (int trav = _tail; trav != _head; ) {
	trav = prev_i(trav);
	if (filter(_q[trav])) {
	    yank_vec.push_back(_q[trav]);
	    nyanked++;
	} else {
	    write_ptr = prev_i(write_ptr);
	    _q[write_ptr] = _q[trav];
	}
    }
    _head = write_ptr;
    return nyanked;
}

CLICK_ENDDECLS
#endif
