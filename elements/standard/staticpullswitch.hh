#ifndef CLICK_STATICPULLSWITCH_HH
#define CLICK_STATICPULLSWITCH_HH
#include <click/element.hh>

/*
 * =c
 * StaticPullSwitch(K)
 * =s packet scheduling
 * forwards pull requests to fixed input
 * =d
 *
 * On every pull, StaticPullSwitch returns the packet pulled from one of its
 * input ports -- specifically, input number K. Negative Ks mean always return
 * a null packet. StaticPullSwitch has an unlimited number of inputs.
 *
 * =n
 *
 * StaticPullSwitch differs from PullSwitch in that it has no C<switch> write
 * handler, and thus does not allow K to be changed at run time.
 *
 * =a PullSwitch, PrioSched, RoundRobinSched, StrideSched, Switch */

class StaticPullSwitch : public Element {

  int _input;
  
 public:
  
  StaticPullSwitch();
  ~StaticPullSwitch();
  
  const char *class_name() const		{ return "StaticPullSwitch"; }
  const char *processing() const		{ return PULL; }
  
  StaticPullSwitch *clone() const;
  void notify_ninputs(int);
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *pull(int);
  
};

#endif
