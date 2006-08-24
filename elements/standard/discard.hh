#ifndef CLICK_DISCARD_HH
#define CLICK_DISCARD_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

Discard

=s basicsources

drops all packets

=d

Discards all packets received on its single input. If used in a pull context,
it initiates pulls whenever packets are available, and listens for activity
notification, such as that available from Queue.

=a Queue */

class Discard : public Element { public:
  
  Discard();
  ~Discard();
  
  const char *class_name() const		{ return "Discard"; }
  const char *port_count() const		{ return PORTS_1_0; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int initialize(ErrorHandler *);
  void add_handlers();
  
  void push(int, Packet *);
  bool run_task(Task *);

 protected:

  Task _task;
  NotifierSignal _signal;
  
};

CLICK_ENDDECLS
#endif
