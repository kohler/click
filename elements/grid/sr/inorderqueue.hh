// -*- c-basic-offset: 4 -*-
#ifndef CLICK_INORDERQUEUE_HH
#define CLICK_INORDERQUEUE_HH
#include <elements/standard/notifierqueue.hh>
#include <click/bighashmap.hh>
#include <elements/grid/sr/path.hh>
CLICK_DECLS

/*
=c

InOrderQueue
InOrderQueue(CAPACITY)

=s storage

stores packets in a queue

=a Queue, SimpleQueue, FrontDropQueue */

class InOrderQueue : public NotifierQueue { public:
    
    InOrderQueue();
    ~InOrderQueue();
    
    const char *class_name() const	{ return "InOrderQueue"; }
    void *cast(const char *);
    InOrderQueue *clone() const		{ return new InOrderQueue; }
    const char *processing() const	{ return "hhh/lh"; }
    inline bool enq(Packet *p);
    Packet *pull(int);
    void push(int port, Packet *);
    int configure(Vector<String> &, ErrorHandler *);    
    void run_timer();
private:
    int _drops;

    class FlowTableEntry {
    public:
	class IPFlowID _id;
	struct timeval _last_tx;
	tcp_seq_t _th_seq;
	FlowTableEntry() { }
	FlowTableEntry(IPFlowID id) : _id(id) { }

	struct timeval last_tx_age() {
	    struct timeval age;
	    struct timeval now;
	    click_gettimeofday(&now);
	    timersub(&now, &_last_tx, &age);
	    return age;
	}
    };
    
    typedef BigHashMap<IPFlowID, FlowTableEntry> FlowTable;
    typedef FlowTable::const_iterator FlowIter;
    FlowTable _flows;
    

    struct yank_filter {
	InOrderQueue *s;
	yank_filter(InOrderQueue *t) {
	    s = t;
	}
	bool operator()(const Packet *p) {
	    return (s) ? s->ready_for(p) : false;
	}
    };

    bool ready_for(const Packet *);
    int bubble_up(Packet *);
    String print_stats();
    static String static_print_stats(Element *, void *);
    void add_handlers();
    static int static_clear(const String &arg, Element *e,
			    void *, ErrorHandler *errh); 
    void clear();
    static int static_write_debug(const String &arg, Element *e,
				  void *, ErrorHandler *errh); 
    
    static int static_write_packet_timeout(const String &arg, Element *e,
					   void *, ErrorHandler *errh); 
    int set_packet_timeout(ErrorHandler *, unsigned int);

    static String static_print_debug(Element *, void *);
    static String static_print_packet_timeout(Element *, void *);
    
    bool _debug;
    unsigned int _max_tx_packet_ms;
    struct timeval _packet_timeout;
    Timer _timer;
};

inline bool
InOrderQueue::enq(Packet *p)
{
    assert(p);
    int next = next_i(_tail);
    if (next != _head) {
	_q[_tail] = p;
	_tail = next;
	return true;
    } else
	p->kill();

    return false;
}

CLICK_ENDDECLS
#endif
