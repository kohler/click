#ifndef CLICK_PAINTTEE_HH
#define CLICK_PAINTTEE_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * PaintTee(X)
 * =s duplication
 * duplicates packets with given paint annotation
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
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

CLICK_ENDDECLS
#endif
