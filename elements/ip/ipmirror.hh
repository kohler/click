#ifndef IPMIRROR_HH
#define IPMIRROR_HH
#include <click/element.hh>

/*
 * =c
 * IPMirror
 * =s swaps IP source and destination
 * V<modifies>
 * =d
 *
 * Has one input, one output.  Incoming packets a->b are pushed out as b->a.
 *
 * =a Rewriter
 */

class IPMirror : public Element {

 public:

  IPMirror();
  ~IPMirror();
  
  const char *class_name() const		{ return "IPMirror"; }
  const char *processing() const		{ return AGNOSTIC; }
  IPMirror *clone() const			{ return new IPMirror; }
  
  Packet *simple_action(Packet *);
  
};

#endif IPMIRROR_HH
