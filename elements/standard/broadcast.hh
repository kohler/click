#ifndef BROADCAST_HH
#define BROADCAST_HH
#include "element.hh"

/*
 * =c
 * Broadcast()
 * =io
 * Has one input and an unlimited number of outputs.
 * =d
 * Sends a copy of each input packet to every output.
 * =a Tee
 */

class Broadcast : public Element {
  
 public:
  
  Broadcast()					{ }
  ~Broadcast()					{ }
  
  const char *class_name() const		{ return "Broadcast"; }
  const char *processing() const		{ return PUSH; }
  void notify_noutputs(int);
  
  Broadcast *clone() const;
  
  void push(int port, Packet* p);
  
};

#endif
