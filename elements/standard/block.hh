#ifndef BLOCK_HH
#define BLOCK_HH
#include "element.hh"

/*
 * =c
 * Block()
 * =d
 * Blocks packets based on the sibling annotation set by the Monitor.
 *
 * =e
 * = ... -> Block() -> ...
 *
 * =h
 *
 * =a RED
 * =a Monitor
 */

class Block : public Element {
  
 public:
  
  Block();
  Block *clone() const;

  const char *class_name() const		{ return "Block"; }
  const char *processing() const	        { return AGNOSTIC; }
  void add_handlers();
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  // bool can_live_reconfigure() const		{ return true; }
  
  void push(int port, Packet *);
  // Packet *pull(int port);

 private:

  int _block;
  
};

#endif
