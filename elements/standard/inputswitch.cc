#include <click/config.h>
#include "inputswitch.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

InputSwitch::InputSwitch()
{
}

int
InputSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int input = 0;
    if (Args(conf, this, errh).read_p("OUTPUT", input).complete() < 0)
	return -1;
    if (input >= ninputs())
	return errh->error("input must be < %d", ninputs());
    _input = input;
    return 0;
}

void
InputSwitch::push(int port, Packet *p)
{
    if (port == _input)
	output(0).push(p);
    else
	p->kill();
}

String
InputSwitch::read_param(Element *e, void *)
{
    InputSwitch *sw = (InputSwitch *)e;
    return String(sw->_input);
}

int
InputSwitch::write_param(const String &s, Element *e, void *, ErrorHandler *errh)
{
    InputSwitch *sw = (InputSwitch *)e;
    int sw_input;
    if (!IntArg().parse(s, sw_input))
	return errh->error("InputSwitch input must be integer");
    if (sw_input >= sw->ninputs())
	sw_input = -1;
    sw->_input = sw_input;
    return 0;
}

void
InputSwitch::add_handlers()
{
    add_read_handler("switch", read_param, 0);
    add_write_handler("switch", write_param, 0, Handler::h_nonexclusive);
    add_read_handler("config", read_param, 0);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(InputSwitch)
ELEMENT_MT_SAFE(InputSwitch)
