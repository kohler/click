#ifndef CLICK_DISCARD_HH
#define CLICK_DISCARD_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>

/*
 * =c
 * Discard
 * =s dropping
 * drops all packets
 * =d
 * Discards all packets received on its single input.
 * If used in a pull context, it initiates pulls whenever
 * packets are available, and listens for activity notification; see
 * NotifierQueue.
 */

class Discard : public Element { public:
  
  Discard();
  ~Discard();
  
  const char *class_name() const		{ return "Discard"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Discard *clone() const			{ return new Discard; }
  int initialize(ErrorHandler *);
  void add_handlers();
  
  void push(int, Packet *);
  void run_scheduled();

 protected:

  Task _task;
  NotifierSignal _signal;
  
};

#endif
