// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_PI_HH
#define CLICK_PI_HH
#include <click/element.hh>
#include <click/ewma64.hh>
#include <click/timer.hh>
CLICK_DECLS
class Storage;

class PI : public Element { public:

    PI();
    ~PI();

    const char *class_name() const		{ return "PI"; }
    const char *processing() const		{ return "a/ah"; }
    PI *clone() const;

    int queue_size() const;
    const DirectEWMA64 &average_queue_size() const { return _size; }
    int drops() const				{ return _drops; }

    void notify_noutputs(int);
    int configure(Vector<String> &, ErrorHandler *);
    int check_params(double, double, double, unsigned, unsigned, ErrorHandler *) const ;
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void take_state(Element *, ErrorHandler *);
    void configuration(Vector<String> &) const;
    bool can_live_reconfigure() const		{ return true; }
    int live_reconfigure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    bool should_drop();
    void handle_drop(Packet *);
    void push(int port, Packet *);
    Packet *pull(int port);
    void run_scheduled();

  protected:
	
	Timer _timer;
    Storage *_queue1;
    Vector<Storage *> _queues;

    // Queue sizes are shifted by this much.
    static const unsigned QUEUE_SCALE = 10;

    DirectEWMA64 _size;

    int _random_value;
    int _last_jiffies;

    int _drops;

	double _a, _b, _w, _p; 
	unsigned _target_q, _old_q;

    Vector<Element *> _queue_elements;

    static String read_stats(Element *, void *);
    static String read_queues(Element *, void *);
    static String read_parameter(Element *, void *);

    static const int MAX_RAND=2147483647;

};

CLICK_ENDDECLS
#endif
