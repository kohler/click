#ifndef ALIGN_HH
#define ALIGN_HH
#include <click/element.hh>

/* =c
 * Align(MODULUS, OFFSET)
 * =s
 * aligns packet data
 * V<modification>
 * =d
 *
 * Aligns packet data. Each input packet is aligned so that its first byte is
 * OFFSET bytes off from a MODULUS-byte boundary. This may involve a packet
 * copy.
 *
 * MODULUS must be 2, 4, or 8.
 * =n
 *
 * The click-align(1) tool will insert this element automatically wherever it
 * is required.
 *
 * =e
 *   ... -> Align(4, 0) -> ...
 *
 * =a AlignmentInfo, click-align(1) */

class Align : public Element {

  int _offset;
  int _mask;
  
 public:
  
  Align();
  ~Align();
  
  const char *class_name() const		{ return "Align"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Align *clone() const				{ return new Align; }
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);
  
};

#endif
