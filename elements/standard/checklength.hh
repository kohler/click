#ifndef CHECKLENGTH_HH
#define CHECKLENGTH_HH
#include "element.hh"

/*
 * =c
 * CheckLength(MAX)
 * =d
 * CheckLength checks every packet's length against MAX. If the packet has
 * length MAX or smaller, it is sent to output 0; otherwise, it is sent to output 1 (or dropped if there is no output 1).
 *
 */

class CheckLength : public Element {
  
  unsigned _max;
  
 public:
  
  CheckLength()					: Element(1, 1) { }
  
  const char *class_name() const		{ return "CheckLength"; }
  void notify_noutputs(int);
  void processing_vector(Vector<int> &, int, Vector<int> &, int) const;
  
  CheckLength *clone() const			{ return new CheckLength; }
  int configure(const String &, ErrorHandler *);
  
  void push(int, Packet *);
  Packet *pull(int);
  
};

#endif
