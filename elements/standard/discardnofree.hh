#ifndef DISCARDNOFREE_HH
#define DISCARDNOFREE_HH
#include <click/element.hh>

/*
 * =c
 * DiscardNoFree
 * =s
 * drops all packets, but does not free any of them.
 * V<dropping>
 * =d
 * Discards all packets received on its single input, but does not free any of
 * them. Only useful for benchmarking.
 */

class DiscardNoFree : public Element {
  
 public:
  
  DiscardNoFree();
  ~DiscardNoFree();
  
  const char *class_name() const		{ return "DiscardNoFree"; }
  const char *processing() const		{ return AGNOSTIC; }
  DiscardNoFree *clone() const			{ return new DiscardNoFree; }
  
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void push(int, Packet *);
  
  void run_scheduled();
  
};

#endif
