#ifndef CLICK_DISCARDNOFREE_HH
#define CLICK_DISCARDNOFREE_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
 * =c
 * DiscardNoFree
 * =s dropping
 * drops all packets, but does not free any of them.
 * =d
 * Discards all packets received on its single input, but does not free any of
 * them. Only useful for benchmarking.
 */

class DiscardNoFree : public Element { public:
  
  DiscardNoFree();
  ~DiscardNoFree();
  
  const char *class_name() const		{ return "DiscardNoFree"; }
  const char *processing() const		{ return AGNOSTIC; }
  DiscardNoFree *clone() const			{ return new DiscardNoFree; }
  
  int initialize(ErrorHandler *);
  void add_handlers();
  
  void push(int, Packet *);
  void run_scheduled();

 private:

  Task _task;
  
};

CLICK_ENDDECLS
#endif
