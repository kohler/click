#ifndef CLICK_DELAYUNQUEUE_HH
#define CLICK_DELAYUNQUEUE_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include "activity.hh"

/*
=c
DelayUnqueue(DELAY)
=s packet scheduling
pull-to-push converter
=d

Pulls packets from the single input port. Delays them for at least DELAY
seconds, with microsecond precision. A packet with timestamp T will be emitted
no earlier than time (T + DELAY). On output, the packet's timestamp is set to
the current time.

DelayUnqueue listens for activity notification; see NotifierQueue.

=a Queue, Unqueue, RatedUnqueue, BandwidthRatedUnqueue, SetTimestamp,
NotifierQueue */

class DelayUnqueue : public Element { public:
  
  DelayUnqueue();
  ~DelayUnqueue();

  const char *class_name() const	{ return "DelayUnqueue"; }
  const char *processing() const	{ return PULL_TO_PUSH; }
  DelayUnqueue *clone() const		{ return new DelayUnqueue; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  void run_scheduled();
  static String read_param(Element *e, void *);

 private:

  Packet *_p;
  struct timeval _delay;
  ActivitySignal _signal;
  Task _task;
  Timer _timer;
  
};

#endif
