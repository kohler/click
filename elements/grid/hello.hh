#ifndef HELLO_HH
#define HELLO_HH

/*
 * =c
 * Hello(PERIOD, JITTER, ETH, IP, NeighborName [, MAX-HOPS])
 * =d
 *
 * Every PERIOD millseconds (+/- a jitter bounded by JITTER
 * milliseconds), emit a Grid protocol ``Hello'' packet for the Grid
 * node at address IP with MAC address ETH.  PERIOD must be greater
 * than 0, JITTER must be positive and less than JITTER.  Produces
 * Ethernet packets.  Hello also advertises any neighbors within
 * MAX-HOPS of the node, as reported by the Neighbor element
 * named by the 5th argument.  MAX-HOPS defaults to 1.
 *
 * =e
 * Hello(500, 100, 00:E0:98:09:27:C5, 18.26.4.115, nel) -> ? -> ToDevice(eth0)
 *
 * =a Neighbor */

#include "element.hh"
#include "timer.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "neighbor.hh"

class Hello : public Element {
  
public:
  
  Hello();
  ~Hello();
  
  const char *class_name() const		{ return "Hello"; }
  const char *processing() const		{ return PUSH; }
  Hello *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  Packet *make_hello();
  
  void run_scheduled();
  
private:
  
  EtherAddress _from_eth;
  IPAddress _from_ip;
  int _period;
  int _jitter;
  Timer _timer;
  Neighbor *_nbr;
  int _hops;
};

#endif

