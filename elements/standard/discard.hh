#ifndef DISCARD_HH
#define DISCARD_HH
#include "element.hh"

/*
 * =c
 * Discard()
 * =d
 * Discards all packets received on its single input.
 * If used in a Pull context, it initiates pulls whenever
 * packets are available.
 */

class Discard : public Element {
  
 public:
  
  Discard();
  ~Discard()					{ }
  
  const char *class_name() const		{ return "Discard"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Discard *clone() const			{ return new Discard; }
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void push(int, Packet *p) { p->kill(); }
  
  void run_scheduled();
  
};

#endif
