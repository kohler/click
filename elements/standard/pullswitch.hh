#ifndef PULLSWITCH_HH
#define PULLSWITCH_HH
#include <click/element.hh>

/*
 * =c
 * PullSwitch([K])
 * =s forwards pull requests to settable input
 * V<packet scheduling>
 * =d
 *
 * On every pull, PullSwitch returns the packet pulled from one of its input
 * ports -- specifically, input number K. The default K is zero; negative Ks
 * mean always return a null packet. You can change K with a write handler.
 * PullSwitch has an unlimited number of inputs.
 *
 * =h switch read/write
 * Return or set the K parameter.
 *
 * =a StaticPullSwitch, PrioSched, RoundRobinSched, StrideSched, Switch
 */

class PullSwitch : public Element {

  int _input;

  static String read_param(Element *, void *);
  static int write_param(const String &, Element *, void *, ErrorHandler *);
  
 public:
  
  PullSwitch()					{ add_output(); }
  
  const char *class_name() const		{ return "PullSwitch"; }
  const char *processing() const		{ return PULL; }
  void add_handlers();
  
  PullSwitch *clone() const;
  void notify_ninputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  
  Packet *pull(int);
  
};

#endif
