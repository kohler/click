#ifndef PAINTTEE_HH
#define PAINTTEE_HH
#include <click/element.hh>

/*
 * =c
 * PaintTee(X)
 * =s
 * duplicates packets with given paint annotation
 * V<duplication>
 * =d
 * PaintTee sends every packet through output 0. If the packet's
 * color annotation is equal to X (an integer), it also
 * sends a copy through output 1.
 *
 * =e
 * Intended to produce redirects in conjunction with Paint and
 * ICMPError as follows:
 *
 *   FromDevice(eth7) -> Paint(7) -> ...
 *   routingtable[7] -> pt :: PaintTee(7) -> ... -> ToDevice(eth7)
 *   pt[1] -> ICMPError(18.26.4.24, 5, 1) -> [0]routingtable;
 *
 * =a Paint, ICMPError
 */

class PaintTee : public Element {
  
  int _color;
  
 public:
  
  PaintTee();
  ~PaintTee();
  
  const char *class_name() const	{ return "PaintTee"; }
  const char *processing() const	{ return "a/ah"; }
  
  PaintTee *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
