#ifndef SWITCH_HH
#define SWITCH_HH
#include "element.hh"

/*
 * =c
 * Switch([K])
 * =s sends packet stream to settable output V<classifies>
 * =d
 *
 * Switch sends every incoming packet to one of its output ports --
 * specifically, output number K. The default K is zero; negative K means to
 * destroy input packets instead of forwarding them. You can change K with a
 * write handler. Switch has an unlimited number of outputs.
 *
 * =h switch read/write
 * Return or set the K parameter.
 *
 * =a StrideSwitch, PullSwitch */

class Switch : public Element {

  int _output;

  static String read_param(Element *, void *);
  static int write_param(const String &, Element *, void *, ErrorHandler *);
  
 public:
  
  Switch()					{ add_input(); }
  
  const char *class_name() const		{ return "Switch"; }
  const char *processing() const		{ return PUSH; }
  void add_handlers();
  
  Switch *clone() const;
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  
  void push(int, Packet *);
  
};

#endif
