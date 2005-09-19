#ifndef LOOKUPIPROUTE2_HH
#define LOOKUPIPROUTE2_HH
#include <click/glue.hh>
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * LookupIPRoute2()
 * 
 * =s IP, classification
 * simple dynamic IP routing table
 * =d
 * 
 * This element has been renamed as RadixIPLookup.
 *
 * =a RadixIPLookup
 */

class LookupIPRoute2 : public Element {
public:
  LookupIPRoute2();
  ~LookupIPRoute2();
  
  const char *class_name() const		{ return "LookupIPRoute2"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  int configure(Vector<String> &, ErrorHandler *);
};

CLICK_ENDDECLS
#endif
