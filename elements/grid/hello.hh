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

class SendGridHello : public Element {
  
public:
  
  SendGridHello();
  ~SendGridHello();
  
  const char *class_name() const		{ return "SendGridHello"; }
  const char *processing() const		{ return PUSH; }
  SendGridHello *clone() const;
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  Packet *make_hello();
  
  void run_scheduled();
  
private:
  
  EtherAddress _from_eth;
  IPAddress _from_ip;
  int _period;
  int _jitter;
  Timer _timer;
};

#endif

