#ifndef ALIGNMENTINFO_HH
#define ALIGNMENTINFO_HH

/*
 * =c
 * AlignmentInfo(ELEMENT [MODULUS OFFSET ...], ...)
 * =s specifies alignment information
 * =io
 * None
 * =d
 * Provides information about the packet alignment specified elements can
 * expect. Each configuration argument has the form
 * `ELEMENT [MODULUS0 OFFSET0 MODULUS1 OFFSET1 ...]',
 * where there are zero or more MODULUS-OFFSET pairs.
 * All packets arriving at ELEMENT's
 * I<n>th input will start `OFFSETI<n>' bytes
 * off from a `MODULUSI<n>'-byte boundary.
 * =n
 * This element is inserted automatically by click-align(1).
 * =a Align, click-align(1)
 */

#include "element.hh"

class AlignmentInfo : public Element {

  Vector<int> _elem_offset;
  Vector<int> _elem_icount;
  Vector<int> _chunks;
  Vector<int> _offsets;
  
 public:
  
  AlignmentInfo();
  
  const char *class_name() const	{ return "AlignmentInfo"; }
  
  AlignmentInfo *clone() const		{ return new AlignmentInfo; }
  int configure_phase() const		{ return CONFIGURE_PHASE_INFO; }
  int configure(const Vector<String> &, ErrorHandler *);

  bool query1(Element *, int port, int &chunk, int &offset) const;
  static bool query(Element *, int port, int &chunk, int &offset);
  
};

#endif
