#ifndef B8B10_HH
#define B8B10_HH

/*
 * B8B10(flag)
 * 
 * If flag is 1, encode each packet with a 8b10b code.
 * If flag is 0, decode.
 *
 * Encodes each 8-bit byte into a 10-bit symbol with as
 * many 0s as 1s. The point is to keep the BIM-4xx-RS232
 * radio happy.
 */

#include "element.hh"

class B8B10 : public Element {
public:
  B8B10();
  ~B8B10();

  const char *class_name() const		{ return "B8B10"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  int configure(const String &, Router *, ErrorHandler *);
  int initialize(Router *, ErrorHandler *);
  
  B8B10 *clone() const { return(new B8B10()); }
  
  Packet *simple_action(Packet *);

private:
  int _flag;

};

#endif
