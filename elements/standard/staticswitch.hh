#ifndef STATICSWITCH_HH
#define STATICSWITCH_HH
#include <click/element.hh>

/*
 * =c
 * StaticSwitch(K)
 * =s classification
 * sends packet stream to fixed output
 * =d
 *
 * StaticSwitch sends every incoming packet to one of its output ports --
 * specifically, output number K. Negative K means to destroy input packets
 * instead of forwarding them. StaticSwitch has an unlimited number of
 * outputs.
 *
 * =n
 * StaticSwitch differs from Switch in that it has no C<switch> write handler,
 * and thus does not allow K to be changed at run time.
 *
 * =a Switch, StrideSwitch, PullSwitch */

class StaticSwitch : public Element {

  int _output;

 public:
  
  StaticSwitch();
  ~StaticSwitch();
  
  const char *class_name() const		{ return "StaticSwitch"; }
  const char *processing() const		{ return PUSH; }
  
  StaticSwitch *clone() const;
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int, Packet *);
  
};

#endif
