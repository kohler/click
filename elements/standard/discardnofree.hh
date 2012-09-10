#ifndef CLICK_DISCARDNOFREE_HH
#define CLICK_DISCARDNOFREE_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
 * =c
 * DiscardNoFree
 * =s basicsources
 * drops all packets, but does not free any of them.
 * =d
 * Discards all packets received on its single input, but does not free any of
 * them. Only useful for benchmarking.
 */

class DiscardNoFree : public Element { public:

  DiscardNoFree() CLICK_COLD;

  const char *class_name() const		{ return "DiscardNoFree"; }
  const char *port_count() const		{ return PORTS_1_0; }

  int initialize(ErrorHandler *) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  void push(int, Packet *);
  bool run_task(Task *);

 private:

  Task _task;

};

CLICK_ENDDECLS
#endif
