#ifndef CHECKPAINT_HH
#define CHECKPAINT_HH
#include "element.hh"

/*
 * =c
 * CheckPaint(X)
 * =s
 * old name for PaintTee
 * V<duplication>
 * =d
 * This is the old name for the PaintTee element. You should use PaintTee
 * instead.
 * =a PaintTee */

class CheckPaint : public Element {
  int _color;
  
 public:
  
  CheckPaint()				: Element(1, 2) { }
  
  const char *class_name() const	{ return "CheckPaint"; }
  const char *processing() const	{ return "a/ah"; }
  
  CheckPaint *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
