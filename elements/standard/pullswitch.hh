#ifndef CLICK_PULLSWITCH_HH
#define CLICK_PULLSWITCH_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

PullSwitch([K])

=s packet scheduling

forwards pull requests to settable input

=d

On every pull, PullSwitch returns the packet pulled from one of its input
ports -- specifically, input number K. The default K is zero; negative Ks
mean always return a null packet. You can change K with a write handler.
PullSwitch has an unlimited number of inputs.

=h switch read/write

Return or set the K parameter.

=h CLICK_LLRPC_GET_SWITCH llrpc

Argument is a pointer to an integer, in which the Switch's K parameter is
stored.

=h CLICK_LLRPC_SET_SWITCH llrpc

Argument is a pointer to an integer. Sets the K parameter to that integer.

=a StaticPullSwitch, PrioSched, RoundRobinSched, StrideSched, Switch */

class PullSwitch : public Element { public:

  PullSwitch();
  ~PullSwitch();
  
  const char *class_name() const		{ return "PullSwitch"; }
  const char *processing() const		{ return PULL; }
  PullSwitch *clone() const;

  void notify_ninputs(int);
  int configure(Vector<String> &, ErrorHandler *);
  void configuration(Vector<String> &) const;
  bool can_live_reconfigure() const		{ return true; }
  void add_handlers();  
  
  Packet *pull(int);

  int llrpc(unsigned, void *);
  
 private:

  int _input;

  static String read_param(Element *, void *);
  static int write_param(const String &, Element *, void *, ErrorHandler *);
  
};

CLICK_ENDDECLS
#endif
