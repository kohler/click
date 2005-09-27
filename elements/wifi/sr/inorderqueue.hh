// -*- c-basic-offset: 4 -*-
#ifndef CLICK_INORDERQUEUE_HH
#define CLICK_INORDERQUEUE_HH
#include <elements/standard/notifierqueue.hh>
#include <click/bighashmap.hh>
#include <elements/wifi/sr/path.hh>
#include <elements/wifi/sr/srpacket.hh>
CLICK_DECLS

/*
=c

InOrderQueue
InOrderQueue(CAPACITY)

=s storage

stores packets in a push-to-push queue.

=a Queue, SimpleQueue, FrontDropQueue */

class InOrderQueue : public NotifierQueue { public:
    
    InOrderQueue();
    ~InOrderQueue();
    
    const char *class_name() const	{ return "InOrderQueue"; }
    void *cast(const char *);
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return "h/h"; }
    inline bool enq(Packet *p);
    void push(int port, Packet *);
    void shove_out();
    int configure(Vector<String> &, ErrorHandler *);    
    void run_timer(Timer *);
private:
    int _drops;

    class PathInfo {
    public:
	Path _p;
	uint32_t _seq;
	Timestamp _last_tx;

	PathInfo() : _p(), _seq(0) { }
	PathInfo(Path p) : _p(p), _seq(0) { }

	Timestamp last_tx_age() {
	    return Timestamp::now() - _last_tx;
	}
    };
    
    typedef HashMap<Path, PathInfo> PathTable;
    typedef PathTable::const_iterator PathIter;
    PathTable _paths;
    

    struct yank_filter {
	InOrderQueue *s;
	yank_filter(InOrderQueue *t) {
	    s = t;
	}
	bool operator()(const Packet *p) {
	    return (s) ? s->ready_for(p) : false;
	}
    };
    friend class yank_filter;

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

    PathInfo *find_path_info(Path p);
    static String static_print_debug(Element *, void *);
    static String static_print_packet_timeout(Element *, void *);
    
    bool _debug;
    unsigned int _max_tx_packet_ms;
    Timestamp _packet_timeout;
    Timer _timer;
};

inline bool
InOrderQueue::enq(Packet *p)
{
    sr_assert(p);
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
