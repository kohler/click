#ifndef ALIGN_HH
#define ALIGN_HH
#include "element.hh"

/*
 * =c
 * Align(MODULUS, OFFSET)
 * =d
 * Aligns packet data to start OFFSET bytes off from
 * a MODULUS-byte boundary. May involve a packet copy.
 * =e
 * = ... -> Align(4, 0) -> ...
 */

class Align : public Element {

  int _offset;
  int _mask;
  
 public:
  
  Align();
  
  const char *class_name() const		{ return "Align"; }
  Processing default_processing() const		{ return AGNOSTIC; }
  
  Align *clone() const				{ return new Align; }
  int configure(const String &, ErrorHandler *);

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);
  
};

#endif
