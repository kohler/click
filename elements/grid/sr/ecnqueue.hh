// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ECNQUEUE_HH
#define CLICK_ECNQUEUE_HH
#include <elements/standard/notifierqueue.hh>
#include <click/bighashmap.hh>
#include <elements/grid/sr/path.hh>
CLICK_DECLS

/*
=c

ECNQueue
ECNQueue(CAPACITY)

=s storage

stores packets in a push-to-push queue.

=a Queue, SimpleQueue, FrontDropQueue */

class ECNQueue : public NotifierQueue { public:
    
    ECNQueue();
    ~ECNQueue();
    
    const char *class_name() const	{ return "ECNQueue"; }
    void *cast(const char *);
    ECNQueue *clone() const		{ return new ECNQueue; }
    const char *processing() const	{ return "h/l"; }
    inline bool enq(Packet *p);
    void push(int port, Packet *);
    Packet *pull(int);
    int configure(Vector<String> &, ErrorHandler *);    
private:
    int _drops;

    class PathInfo {
    public:
	Path _p;
	int _seq;
	struct timeval _last_tx;
	bool _ecn;
	PathInfo() : _p(), _seq(0) { }
	PathInfo(Path p) : _p(p), _seq(0) { }

	struct timeval last_tx_age() {
	    struct timeval age;
	    struct timeval now;
	    click_gettimeofday(&now);
	    timersub(&now, &_last_tx, &age);
	    return age;
	}
    };
    
    typedef HashMap<Path, PathInfo> PathTable;
    typedef PathTable::const_iterator PathIter;
    PathTable _paths;
    

    int bubble_up(Packet *);
    String print_stats();
    static String static_print_stats(Element *, void *);
    void add_handlers();
    static int static_clear(const String &arg, Element *e,
			    void *, ErrorHandler *errh); 
    void clear();
    static int static_write_debug(const String &arg, Element *e,
				  void *, ErrorHandler *errh); 
    
    PathInfo *find_path_info(Path p);
    static String static_print_debug(Element *, void *);
    
    bool _debug;
};

inline bool
ECNQueue::enq(Packet *p)
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
