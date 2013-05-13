#ifndef CLICK_INPUTSWITCH_HH
#define CLICK_INPUTSWITCH_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

InputSwitch([INPUT])

=s classification

accepts packet stream from settable input

=d

InputSwitch only accepts incoming packet on one of its input ports --
specifically, INPUT. The default INPUT is zero; negative INPUT means to
destroy input packets instead of forwarding them. You can change INPUT with a
write handler. InputSwitch has an unlimited number of inputs.

=h switch read/write

Return or set the INPUT parameter.

=a Switch, PullSwitch */

class InputSwitch : public Element { public:

    InputSwitch() CLICK_COLD;

    const char *class_name() const		{ return "InputSwitch"; }
    const char *port_count() const		{ return "-/1"; }
    const char *processing() const		{ return PUSH; }
    void add_handlers() CLICK_COLD;

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }

    void push(int, Packet *);

 private:

    int _input;

    static String read_param(Element *, void *) CLICK_COLD;
    static int write_param(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;
};

CLICK_ENDDECLS
#endif
