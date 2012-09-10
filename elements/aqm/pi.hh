// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_PI_HH
#define CLICK_PI_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/timer.hh>
CLICK_DECLS
class Storage;

class PI : public Element { public:

    // Queue sizes are shifted by this much.
    enum { QUEUE_SCALE = 10 };
    typedef DirectEWMAX<StabilityEWMAXParameters<QUEUE_SCALE, uint64_t, int64_t> > ewma_type;

    PI() CLICK_COLD;
    ~PI() CLICK_COLD;

    const char *class_name() const		{ return "PI"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int queue_size() const;
    const ewma_type &average_queue_size() const { return _size; }
    int drops() const				{ return _drops; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int check_params(double, double, double, unsigned, unsigned, ErrorHandler *) const ;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void take_state(Element *, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
    int live_reconfigure(Vector<String> &, ErrorHandler *);
    void add_handlers() CLICK_COLD;

    bool should_drop();
    void handle_drop(Packet *);
    void push(int port, Packet *);
    Packet *pull(int port);
    void run_timer(Timer *);

  protected:

	Timer _timer;
    Storage *_queue1;
    Vector<Storage *> _queues;

    ewma_type _size;

    int _random_value;
    int _last_jiffies;

    int _drops;

	double _a, _b, _w, _p;
	unsigned _target_q, _old_q;

    Vector<Element *> _queue_elements;

    static String read_parameter(Element *, void *);

    static const int MAX_RAND=2147483647;

};

CLICK_ENDDECLS
#endif
