#ifndef DELAYUNQUEUE_HH
#define DELAYUNQUEUE_HH
#include <click/element.hh>
#include <click/task.hh>

/*
 * =c
 * DelayUnqueue(DELAY)
 * =s packet scheduling
 * pull-to-push converter
 * =d
 * Pulls packets from input port. queues packet if current timestamp minus
 * packet timestamp is less than DELAY us. otherwise push packet on output.
 *
 * SetTimestamp element can be used to stamp the packet.
 *
 * =a Queue, Unqueue, RatedUnqueue, BandwidthRatedUnqueue
 */

class DelayUnqueue : public Element { public:
  
  DelayUnqueue();
  ~DelayUnqueue();

  const char *class_name() const	{ return "DelayUnqueue"; }
  const char *processing() const	{ return PULL_TO_PUSH; }
  DelayUnqueue *clone() const		{ return new DelayUnqueue; }

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  void run_scheduled();
  static String read_param(Element *e, void *);

 private:

  unsigned _delay;
  Packet *_p;
  Task _task;
};

#endif
