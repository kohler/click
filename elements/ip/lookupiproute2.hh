#ifndef LOOKUPIPROUTE2_HH
#define LOOKUPIPROUTE2_HH

/*
 * =c
 * LookupIPRoute2()
 * =d
 * Input: IP packets (no ether header).
 * Expects a destination IP address annotation with each packet.
 * Looks up the address, sets the destination annotation to
 * the corresponding GW (if non-zero), and emits the packet
 * on its only output.
 *
 * Sets destination annotation based on pokeable routing table.
 *
 * =e
 * This example delivers broadcasts and packets addressed to the local
 * host (18.26.4.24) to itself, packets to net 18.26.4 to the
 * local interface, and all others via gateway 18.26.4.1:
 *
 * = ... -> GetIPAddress(16) -> rt;
 * = rt :: LookupIPRoute2();
 * = rt -> ... -> ToDevice(eth0);
 *
 * =n
 * Encapsultated routing table is pokeable via /proc. Its handlers are called
 * 'add', 'del' and 'look'.
 * 
 *
 * =a LookupIPRoute2
 */

#include "glue.hh"
#include "element.hh"
#include "iptable2.hh"

class LookupIPRoute2 : public Element {
public:
  LookupIPRoute2();
  ~LookupIPRoute2();
  
  const char *class_name() const		{ return "LookupIPRoute2"; }
  Processing default_processing() const		{ return AGNOSTIC; }
  LookupIPRoute2 *clone() const;
  
  void push(int port, Packet *p);

  static int add_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
  static int del_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
  static String look_route_handler(Element *, void *);
  void add_handlers();

private:

  IPTable2 _t;
};

#endif /* LOOKUPIPROUTE2_HH */
