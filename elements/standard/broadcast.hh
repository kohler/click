#ifndef BROADCAST_HH
#define BROADCAST_HH
#include "unlimelement.hh"

/*
 * =c
 * Broadcast()
 * =io
 * Has unlimited inputs and outputs. All of them are push.
 * =d
 * Sends a copy of each input packet to every output.
 * =a Tee
 */

class Broadcast : public UnlimitedElement {
  
 public:
  
  Broadcast()					{ }
  ~Broadcast()					{ }
  
  const char *class_name() const		{ return "Broadcast"; }
  const char *processing() const	{ return PUSH; }
  
  bool unlimited_inputs() const			{ return true; }
  bool unlimited_outputs() const		{ return true; }
  
  Broadcast *clone() const;
  
  void push(int port, Packet* p);
  
};

#endif
