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
 * =s scheduling
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

    const char *class_name() const		{ return "StrideSched"; }
    const char *port_count() const		{ return "1-/1"; }
    const char *processing() const		{ return PULL; }
    const char *flags() const			{ return "S0"; }

    int configure(Vector<String> &conf, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    enum { STRIDE1 = 1U<<16, MAX_TICKETS = 1U<<15 };
    int tickets(int) const;
    int set_tickets(int, int, ErrorHandler *);

    Packet *pull(int port);

  protected:

    struct Client {
	Client **_pprev;
	Client *_next;
	unsigned _pass;
	unsigned _stride;
	int _tickets;
	NotifierSignal _signal;

	Client()
	    : _pprev(0), _next(0), _pass(0), _stride(0), _tickets(-1) {
	}

	void set_tickets(int t) {
	    _tickets = t;
	    _stride = t ? STRIDE1 / t : 0;
	}
	void stride() {
	    _pass += _stride;
	}

	void insert(Client **list) {
	    _pprev = list;
	    while ((_next = *_pprev) && PASS_GT(_pass, _next->_pass))
		_pprev = &_next->_next;
	    *_pprev = this;
	    if (_next)
		_next->_pprev = &_next;
	}
	void remove() {
	    if ((*_pprev = _next))
		_next->_pprev = _pprev;
	}
    };

    Client *_all;
    Client *_list;

    int nclients() const {
	return input_is_pull(0) ? ninputs() : noutputs();
    }
    static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
