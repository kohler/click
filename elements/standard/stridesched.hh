// -*- c-basic-offset: 4 -*-
#ifndef CLICK_STRIDESCHED_HH
#define CLICK_STRIDESCHED_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * StrideSched(TICKETS0, ..., TICKETSI<N-1>)
 * =s packet scheduling
 * pulls from stride-scheduled inputs
 * =d
 * Has one output and N inputs.  Performs simple packet-based stride
 * scheduling, assigning TICKETSI<i> to input I<i> for each input.
 * 
 * Each time a pull comes in on the output, it pulls on its inputs in the order
 * specified by the stride scheduling queue, until all inputs have been tried
 * or one produces a packet.  If an input does not produce a packet, it is not
 * tried again in the current round (for the current pull on the output) even
 * if it has a very short stride.  This minimizes overhead and ensures that
 * an input that produces a packet, if any, is found as soon as possible,
 * consistently with the stride scheduler ordering.
 *
 * The inputs usually come from Queues or other pull schedulers.
 * StrideSched uses notification to avoid pulling from empty inputs.
 *
 * =h tickets0...ticketsI<N-1> read/write
 * Returns or sets the number of tickets for each input port.
 *
 * =a PrioSched, RoundRobinSched, DRRSched, StrideSwitch
 */

class StrideSched : public Element { public:
  
    StrideSched();
    ~StrideSched();

    const char *class_name() const		{ return "StrideSched"; }
    const char *port_count() const		{ return "1-/1"; }
    const char *processing() const		{ return PULL; }
  
    int configure(Vector<String> &conf, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    enum { STRIDE1 = 1U<<16, MAX_TICKETS = 1U<<15 };
    int tickets(int) const;
    int set_tickets(int, int, ErrorHandler *);
  
    Packet *pull(int port);

  protected:
  
    struct Client {
	Client *_prev;
	Client *_next;
	unsigned _pass;
	unsigned _stride;
	int _tickets;
	NotifierSignal _signal;
	int _port;
	Client *_list;
    
	Client() : _prev(0), _next(0), _pass(0), _stride(0), _tickets(-1), _port(-1) { }
	inline Client(int port, int tickets);
    
	void set_tickets(int);
	
	void make_head();
    
	void insert(Client *c);
	void remove();
	void stride();
    };
  
    Client *_list;

};

inline
StrideSched::Client::Client(int port, int tickets)
    : _prev(0), _next(0), _pass(0), _stride(STRIDE1 / tickets),
      _tickets(tickets), _port(port)
{
    _pass = _stride;
}

inline void
StrideSched::Client::make_head()
{
    _prev = _next = _list = this;
}

inline void
StrideSched::Client::insert(Client *c)
{
    assert(this == _list);
    Client *x = _next;
    while (x != _list && PASS_GT(c->_pass, x->_pass))
	x = x->_next;
    // insert c before x
    c->_next = x;
    c->_prev = x->_prev;
    c->_prev->_next = c;
    x->_prev = c;
}

inline void
StrideSched::Client::remove()
{
    _next->_prev = _prev;
    _prev->_next = _next;
    _next = _prev = 0;
}

inline void
StrideSched::Client::set_tickets(int tickets)
{
    _tickets = tickets;
    _stride = STRIDE1 / tickets;
}

inline void
StrideSched::Client::stride()
{
    _pass += _stride;
}

CLICK_ENDDECLS
#endif
