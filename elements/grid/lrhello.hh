#ifndef LRHELLO_HH
#define LRHELLO_HH

/*
 * =c
 * SendGridLRHello(PERIOD, JITTER, ETH, IP, UpdateGridRoutes [, MAX-HOPS])
 * =s Grid
 * =d
 *
 * Every PERIOD millseconds (+/- a jitter bounded by JITTER
 * milliseconds), emit a Grid protocol ``LR_HELLO'' packet for the
 * Grid node at address IP with MAC address ETH, advertising any
 * neighbors within MAX-HOPS of the node, as reported by the
 * UpdateGridRoutes element named by the 5th argument.  MAX-HOPS
 * defaults to 1.  PERIOD must be greater than 0, JITTER must be
 * positive and less than PERIOD.  Produces Grid packets with MAC
 * headers.
 *
 * =e
 * SendGridLRHello(500, 100, 00:E0:98:09:27:C5, 18.26.4.115, nel) -> ? -> ToDevice(eth0)
 *
 * =a
 * UpdateGridRoutes, LookupLocalGridRoute */

#include <click/element.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include "updateroutes.hh"
CLICK_DECLS

class SendGridLRHello : public Element {
  
public:
  
  SendGridLRHello();
  ~SendGridLRHello();
  
  const char *class_name() const		{ return "SendGridLRHello"; }
  const char *processing() const		{ return PUSH; }
  SendGridLRHello *clone() const;
  
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
  UpdateGridRoutes *_nbr;
  int _hops;
};

CLICK_ENDDECLS
#endif
