#ifndef LOOKUPIPROUTE2_HH
#define LOOKUPIPROUTE2_HH

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

#include <click/glue.hh>
#include <click/element.hh>

class LookupIPRoute2 : public Element {
public:
  LookupIPRoute2();
  ~LookupIPRoute2();
  
  const char *class_name() const		{ return "LookupIPRoute2"; }
  const char *processing() const		{ return AGNOSTIC; }
  LookupIPRoute2 *clone() const			{ return new LookupIPRoute2; }
  int configure(Vector<String> &, ErrorHandler *);
};

#endif

