#ifndef PAINT_HH
#define PAINT_HH
#include "element.hh"

/*
 * =c
 * Paint(X)
 * =s sets packet paint annotations
 * =d
 * Sets each packet's paint annotation to X, an integer 0..255.
 * Note that a packet may only be painted with one color.
 * =a PaintTee
 */

class Paint : public Element {
  
  int _color;
  
 public:
  
  Paint();
  ~Paint();
  
  const char *class_name() const		{ return "Paint"; }
  const char *processing() const	{ return AGNOSTIC; }
  Paint *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
};

#endif
