#ifndef CLICK_UNQUEUE2_HH
#define CLICK_UNQUEUE2_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
 * =c
 * Unqueue2([BURSTSIZE])
 * =s packet scheduling
 * pull-to-push converter
 * =d
 * Pulls packets whenever they are available, then pushes them out its single
 * output. Pulls a maximum of BURSTSIZE packets every time it is scheduled,
 * unless downstream queues are full. Default BURSTSIZE is 1. If BURSTSIZE is
 * 0, pull until nothing comes back. Unqueue2 will not pull if there is a
 * downstream queue that is full. It will also limit burst size to equal to
 * the number of available slots in the fullest downstream queue.
 *
 * =a Unqueue, RatedUnqueue, BandwidthRatedUnqueue
 */

class Unqueue2 : public Element { public:
  
  Unqueue2();
  ~Unqueue2();
  
  const char *class_name() const		{ return "Unqueue2"; }
  const char *processing() const		{ return PULL_TO_PUSH; }
  
  Unqueue2 *clone() const			{ return new Unqueue2; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();
  
  void run_scheduled();

  static String read_param(Element *e, void *);

 private:

  int _burst;
  unsigned _packets;
  Task _task;
  Vector<Element*> _queue_elements;

};

CLICK_ENDDECLS
#endif
