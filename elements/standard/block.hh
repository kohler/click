#ifndef BLOCK_HH
#define BLOCK_HH
#include "element.hh"

/*
 * =c
 * Block(THRESH)
 * =d
 * THRESH is an integer.
 *
 * Splits packets based on the sibling annotation set by Monitor. If the
 * annotation is less or equal to THRESH, the packet is pushed on output 0,
 * otherwise on 1.
 *
 * =e
 * = b :: Block(500);
 * = 
 * = ... -> Monitor(...) ->
 * = ... -> b[0] -> ...
 * = ... -> b[1] -> Discard;
 *
 * Discards all packets that are preceded by more than 500 siblings.
 *
 * =h thresh read/write
 * value of THRESH
 *
 * =a RED
 * =a Monitor
 * =a Discard
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

  int _thresh;

  static int thresh_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
  static String thresh_read_handler(Element *e, void *);
  
};

#endif
