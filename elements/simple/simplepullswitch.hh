#ifndef CLICK_SIMPLEPULLSWITCH_HH
#define CLICK_SIMPLEPULLSWITCH_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SimplePullSwitch([INPUT])

=s scheduling

forwards pull requests to settable input, without notification

=d

On every pull, SimplePullSwitch returns the packet pulled from one of its
input ports -- specifically, INPUT. The default INPUT is zero; negative INPUTs
mean always return a null packet. You can change INPUT with a write handler.
PullSwitch has an unlimited number of inputs.

=h switch read/write

Return or set the K parameter.

=h CLICK_LLRPC_GET_SWITCH llrpc

Argument is a pointer to an integer, in which the Switch's K parameter is
stored.

=h CLICK_LLRPC_SET_SWITCH llrpc

Argument is a pointer to an integer. Sets the K parameter to that integer.

=a PullSwitch, StaticPullSwitch, PrioSched, RoundRobinSched, StrideSched,
Switch */

class SimplePullSwitch : public Element { public:

    SimplePullSwitch() CLICK_COLD;
    ~SimplePullSwitch() CLICK_COLD;

    const char *class_name() const		{ return "SimplePullSwitch"; }
    const char *port_count() const		{ return "-/1"; }
    const char *processing() const		{ return PULL; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    virtual void set_input(int input);

    Packet *pull(int);

    int llrpc(unsigned command, void *data);

  protected:

    int _input;

    static String read_param(Element *, void *) CLICK_COLD;
    static int write_param(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
