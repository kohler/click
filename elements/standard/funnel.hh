#ifndef FUNNEL_HH
#define FUNNEL_HH
#include "element.hh"

/*
 * =c
 * Funnel([N])
 *
 * Funnel sends a packet coming on one of its inputs to the output.
 * The optional argument controls how many inputs the Funnel
 * has, and defaults to 2.
 *
 * =e
 * = tee :: Tee;
 * = fun :: Funnel;
 * = dis :: Discard;
 * = ...
 * = -> tee[0] -> [0]fun -> dis;
 * =    tee[1] -> [1]fun -> dis;
 * = ...
 *
 * Note that all occurrences of Funnel's output must, by definition, connect to
 * the same physical element.
 *
 *
 * =a Tee
 */

class Funnel : public Element {
  
 public:
  
  Funnel()						: Element(2, 1) { }
  
  const char *class_name() const		{ return "Funnel"; }
  const char *processing() const		{ return PUSH; }
  
  Funnel *clone() const;
  int configure(const String &, ErrorHandler *);
  
  void push(int, Packet *);
};

#endif
