#ifndef SCRAMBLE_HH
#define SCRAMBLE_HH

/*
 * Scramble()
 * 
 * Exclusive-or each packet with a fixed repeating
 * sequence of bits. The point is to try to end up
 * with a balanced number of ones and zeroes so that
 * simple transmission equipment will be happy.
 * The BIM radio, for example.
 *
 * Scramble is its own inverse.
 */

#include "element.hh"

class Scramble : public Element {
public:
  Scramble();
  ~Scramble();

  const char *class_name() const		{ return "Scramble"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  Scramble *clone() const { return(new Scramble()); }
  
  Packet *simple_action(Packet *);

};

#endif
