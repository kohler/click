#ifndef CLICK_SWITCH_HH
#define CLICK_SWITCH_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Switch([OUTPUT])

=s classification

sends packet stream to settable output

=d

Switch sends every incoming packet to one of its output ports --
specifically, OUTPUT. The default OUTPUT is zero; negative OUTPUT means to
destroy input packets instead of forwarding them. You can change OUTPUT with a
write handler. Switch has an unlimited number of outputs.

=h switch read/write

Return or set the OUTPUT parameter.

=h CLICK_LLRPC_GET_SWITCH llrpc

Argument is a pointer to an integer, in which the Switch's K parameter is
stored.

=h CLICK_LLRPC_SET_SWITCH llrpc

Argument is a pointer to an integer. Sets the K parameter to that integer.

=a StaticSwitch, PullSwitch, RoundRobinSwitch, StrideSwitch, HashSwitch,
RandomSwitch */

class Switch : public Element { public:

  Switch();

  const char *class_name() const		{ return "Switch"; }
  const char *port_count() const		{ return "1/-"; }
  const char *processing() const		{ return PUSH; }
  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  void push(int, Packet *);

  int llrpc(unsigned, void *);

 private:

  int _output;

  static String read_param(Element *, void *);
  static int write_param(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
