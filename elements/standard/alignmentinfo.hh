#ifndef ALIGNMENTINFO_HH
#define ALIGNMENTINFO_HH

/*
 * =c
 * AlignmentInfo(XXX, ...)
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
