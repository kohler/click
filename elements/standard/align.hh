#ifndef ALIGN_HH
#define ALIGN_HH
#include "element.hh"

/*
 * =c
 * Align(OFFSET [, MODULUS])
 * =d
 * Aligns packet data to start OFFSET bytes off from
 * a MODULUS-byte boundary. May involve a packet copy. The default
 * MODULUS is 4.
 * =e
 * = ... -> Align(0) -> ...
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
