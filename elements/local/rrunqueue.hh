#ifndef RRUNQUEUE_HH
#define RRUNQUEUE_HH
#include <click/element.hh>
#include <click/task.hh>

/*
 * =c
 * RoundRobinUnqueue([BURSTSIZE])
 * =s packet scheduling
 * pull-to-push converter
 * =d
 * Pulls packets from input ports in a round robin fashion, then pushes them
 * out the output corresponding to the input that the packet came from. Pulls
 * a maximum of BURSTSIZE packets every time it is scheduled. Default
 * BURSTSIZE is 1. If BURSTSIZE is 0, pull until nothing comes back.
 *
 * =a Unqueue, RatedUnqueue, BandwidthRatedUnqueue
 */

class RoundRobinUnqueue : public Element { public:
  
  RoundRobinUnqueue();
  ~RoundRobinUnqueue();

  void notify_ninputs(int i) 		{ set_ninputs(i); }
  void notify_noutputs(int i) 		{ set_noutputs(i); }

  const char *class_name() const	{ return "RoundRobinUnqueue"; }
  const char *processing() const	{ return PULL_TO_PUSH; }
 
  RoundRobinUnqueue *clone() const	{ return new RoundRobinUnqueue; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();
  
  void run_scheduled();

  static String read_param(Element *e, void *);

 private:

  int _burst;
  unsigned _packets;
  Task _task;
  int _next;
};

#endif
