#ifndef CLICK_SIMPLEIDLE_HH
#define CLICK_SIMPLEIDLE_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * SimpleIdle
 * =s basicsources
 * discards packets
 * =d
 *
 * Like Idle, but does not provide notification.
 *
 * =sa Idle
 */

class SimpleIdle : public Element { public:

    SimpleIdle() CLICK_COLD;
    ~SimpleIdle() CLICK_COLD;

    const char *class_name() const	{ return "SimpleIdle"; }
    const char *port_count() const	{ return "-/-"; }
    const char *processing() const	{ return "a/a"; }
    const char *flow_code() const	{ return "x/y"; }

    void push(int, Packet *);
    Packet *pull(int);

};

CLICK_ENDDECLS
#endif
