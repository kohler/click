#ifndef LOOKUPIPROUTE2_HH
#define LOOKUPIPROUTE2_HH

/*
 * =c
 * LookupIPRoute2()
 * =s simple dynamic IP routing table
 * V<classification>
 * =d
 * Input: IP packets (no ether header).
 * Expects a destination IP address annotation with each packet. Looks up the
 * address, sets the destination annotation to the corresponding GW (if
 * non-zero), and emits the packet on its only output.
 *
 * Sets destination annotation based on pokeable routing table.
 *
 * =h add write
 * Adds an entry to the routing table. Expects DST MASK GW.
 *
 * =h del write
 * Removes an entry from the routing table. Expects DST MASK.
 *
 * =h look read-only
 * Returns the contents of the routing table.
 *
 * =a LookupIPRoute
 */

#include "glue.hh"
#include "element.hh"
#include "iptable2.hh"

class LookupIPRoute2 : public Element {
public:
  LookupIPRoute2();
  ~LookupIPRoute2();
  
  const char *class_name() const		{ return "LookupIPRoute2"; }
  const char *processing() const		{ return AGNOSTIC; }
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
