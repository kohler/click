#ifndef FRONTDROPQUEUE_HH
#define FRONTDROPQUEUE_HH
#include "queue.hh"

/*
 * =c
 * FrontDropQueue
 * FrontDropQueue(CAPACITY)
 * =d
 * Stores incoming packets in a first-in-first-out queue.
 * Drops the head packet before inserting the incoming packet if the queue already holds CAPACITY packets.
 * The default for CAPACITY is 1000.
 * =h length read-only
 * Returns the current number of packets in the queue.
 * =h highwater_length read-only
 * Returns the maximum number of packets that have ever been in the queue at once.
 * =h capacity read/write
 * Returns or sets the queue's capacity.
 * =h drops read-only
 * Returns the number of packets dropped by the Queue so far.
 * =a Queue, RED
 */

class FrontDropQueue : public Queue {
  
 public:
  
  FrontDropQueue();
  ~FrontDropQueue();
  
  const char *class_name() const		{ return "FrontDropQueue"; }
  void *cast(const char *);
  
  FrontDropQueue *clone() const			{ return new FrontDropQueue; }
  int live_reconfigure(const Vector<String> &, ErrorHandler *);
  void take_state(Element *, ErrorHandler *);
  
  void push(int port, Packet *);
  
};

#endif
