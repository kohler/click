#ifndef DISCARDNOFREE_HH
#define DISCARDNOFREE_HH
#include "element.hh"

/*
 * Like Discard, but doesn't free the packet.
 * Only useful with Spew for benchmarking.
 */

class DiscardNoFree : public Element {
  
 public:
  
  DiscardNoFree();
  
  const char *class_name() const		{ return "DiscardNoFree"; }
  Processing default_processing() const		{ return AGNOSTIC; }
  
  DiscardNoFree *clone() const			{ return new DiscardNoFree; }
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void push(int, Packet *);
  
  void run_scheduled();
  
};

#endif
