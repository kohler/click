#ifndef ALIGNMENTINFO_HH
#define ALIGNMENTINFO_HH

/*
 * =c
 * AlignmentInfo(ELEMENT MODULUS OFFSET, ...)
 * =io
 * None
 * =d
 * Stores information about the packet alignment other elements can
 * expect. Each configuration argument has the form `ELEMENT MODULUS OFFSET',
 * meaning that the element named ELEMENT can expect all input packet data to
 * start OFFSET bytes off from a MODULUS-byte boundary. This element is
 * inserted automatically by the click-align tool.
 * =a Align
 */

#include "element.hh"

class AlignmentInfo : public Element {

  Vector<int> _elem_offset;
  Vector<int> _elem_icount;
  Vector<int> _chunks;
  Vector<int> _offsets;
  
 public:
  
  AlignmentInfo();
  
  const char *class_name() const		{ return "AlignmentInfo"; }
  
  AlignmentInfo *clone() const			{ return new AlignmentInfo; }
  bool configure_first() const;
  int configure(const String &, ErrorHandler *);

  bool query1(Element *, int port, int &chunk, int &offset) const;
  static bool query(Element *, int port, int &chunk, int &offset);
  
};

#endif
