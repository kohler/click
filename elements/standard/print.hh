#ifndef PRINT_HH
#define PRINT_HH
#include "element.hh"
#include "string.hh"

/*
 * =c
 * Print(tag)
 * =d
 * Moves each packet from input 0 to output 0.
 * Prints the first few bytes, in hex, of each packet,
 * preceded by the tag text.
 */

class Print : public Element {
  
  String _label;
  unsigned _bytes;		// How many bytes of a packet to print
  char* _buf;			// To hold print message
  
 public:
  
  Print();
  Print(const String &label);
  ~Print();
  
  const char *class_name() const		{ return "Print"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  Print *clone() const;
  int configure(const String &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
