#ifndef HELLO_HH
#define HELLO_HH

/*
 * =c
 * SendGridHello(PERIOD, JITTER, ETH, IP);
 * =s Grid
 * =d
 *
 * Every PERIOD millseconds (+/- a jitter bounded by JITTER
 * milliseconds), emit a Grid protocol ``Hello'' packet for the Grid
 * node at address IP with MAC address ETH.  PERIOD must be greater
 * than 0, JITTER must be positive and less than JITTER.  Produces
 * Grid packets with MAC headers.
 *
 * FixSrcLoc puts the node's position into the packet.
 *
 * =e
 * SendGridHello(500, 100, 00:E0:98:09:27:C5, 18.26.4.115) -> ? -> ToDevice(eth0)
 *
 * =a
 * UpdateGridRoutes, SendGridLRHello
 */

#include <click/element.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

class SendGridHello : public Element {

public:

  SendGridHello() CLICK_COLD;
  ~SendGridHello() CLICK_COLD;

  const char *class_name() const		{ return "SendGridHello"; }
  const char *port_count() const		{ return PORTS_0_1; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  Packet *make_hello();

  void run_timer(Timer *);

private:

  EtherAddress _from_eth;
  IPAddress _from_ip;
  int _period;
  int _jitter;
  Timer _timer;
};

CLICK_ENDDECLS
#endif
