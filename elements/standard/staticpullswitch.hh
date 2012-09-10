#ifndef CLICK_STATICPULLSWITCH_HH
#define CLICK_STATICPULLSWITCH_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * StaticPullSwitch(INPUT)
 * =s scheduling
 * forwards pull requests to fixed input
 * =d
 *
 * On every pull, StaticPullSwitch returns the packet pulled from one of its
 * input ports -- specifically, INPUT. Negative INPUTs mean always return
 * a null packet. StaticPullSwitch has an unlimited number of inputs.
 *
 * =n
 *
 * StaticPullSwitch differs from PullSwitch in that it has no C<switch> write
 * handler, and thus does not allow INPUT to be changed at run time.
 *
 * =a PullSwitch, PrioSched, RoundRobinSched, StrideSched, Switch */

class StaticPullSwitch : public Element {

  int _input;

 public:

  StaticPullSwitch() CLICK_COLD;

  const char *class_name() const		{ return "StaticPullSwitch"; }
  const char *port_count() const		{ return "-/1"; }
  const char *processing() const		{ return PULL; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *pull(int);

};

CLICK_ENDDECLS
#endif
