#ifndef MIRROR_HH
#define MIRROR_HH

#include "rewriter.hh"

/*
 * =c
 * Mirror()
 * =d
 *
 * Has one input, one output.  Incoming packets a->b are pushed out as b->a.
 *
 * =a Rewriter
 */

class Mirror : public Element {

public:

  Mirror() 					{ add_input(); add_output(); }
  ~Mirror() 					{ }
  Mirror *clone() const				{ return new Mirror(); }
  
  const char *class_name() const		{ return "Mirror"; }
  Processing default_processing() const		{ return PUSH; }
  
  void push(int, Packet *);
};

#endif MIRROR_HH
