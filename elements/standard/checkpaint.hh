#ifndef CHECKPAINT_HH
#define CHECKPAINT_HH
#include "element.hh"

/*
 * =c
 * CheckPaint(X)
 * =s
 * duplicates packets with given paint annotation
 * V<duplication>
 * =d
 * CheckPaint sends every packet through output 0. If the packet's
 * color annotation is equal to X (an integer), CheckPaint also
 * sends a copy through output 1.
 *
 * =e
 * Intended to produce redirects in conjunction with Paint and
 * ICMPError as follows:
 *
 *   FromDevice(eth7) -> Paint(7) -> ...
 *   routingtable[7] -> cp :: CheckPaint(7) -> ... -> ToDevice(eth7)
 *   cp[1] -> ICMPError(18.26.4.24, 5, 1) -> [0]routingtable;
 *
 * =a Paint, ICMPError
 */

class CheckPaint : public Element {
  int _color;
  
 public:
  
  CheckPaint()						: Element(1, 2) { }
  
  const char *class_name() const		{ return "CheckPaint"; }
  const char *processing() const	{ return "a/ah"; }
  
  CheckPaint *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
