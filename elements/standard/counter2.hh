#ifndef COUNTER2_HH
#define COUNTER2_HH
#include "element.hh"
#include "ewma.hh"
#include "timer.hh"

/*
 * =c
 * Counter2()
 * =d
 * Passes packets unchanged from its input to its output, maintaining
 * statistics information about packet count and total duration of packet
 * reception.
 * =h count read-only
 * Returns the number of packets that have passed through.
 * =h duration read-only
 * Returns duration of packet arrival from first to last packet.
 * =h reset write-only
 * Resets the count and rate to zero.
 */

class Counter2 : public Element { protected:
  
  int _count;
  unsigned long _duration;
  struct timeval _last;
  
 public:

  Counter2();
  
  const char *class_name() const		{ return "Counter2"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int count() const				{ return _count; }
  unsigned long duration() const		{ return _duration; }
  void reset();
  
  Counter2 *clone() const			{ return new Counter2; }
  int initialize(ErrorHandler *);
  void add_handlers();
  
  /*void push(int port, Packet *);
    Packet *pull(int port);*/
  Packet *simple_action(Packet *);
  
};

#endif
