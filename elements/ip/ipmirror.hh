#ifndef IPMIRROR_HH
#define IPMIRROR_HH
#include "element.hh"

/*
 * =c
 * IPMirror()
 * =d
 *
 * Has one input, one output.  Incoming packets a->b are pushed out as b->a.
 *
 * =a Rewriter
 */

class IPMirror : public Element {

 public:

  IPMirror() 					{ add_input(); add_output(); }
  IPMirror *clone() const			{ return new IPMirror(); }
  
  const char *class_name() const		{ return "IPMirror"; }
  Processing default_processing() const		{ return PUSH; }
  
  void push(int, Packet *);
  
};

#endif IPMIRROR_HH
