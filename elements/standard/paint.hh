#ifndef CLICK_PAINT_HH
#define CLICK_PAINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Paint(X)

=s annotations

sets packet paint annotations

=d

Sets each packet's paint annotation to X, an integer 0..255. Note that a
packet may only be painted with one color.


=h color read/write

get/set the color to paint

=n

The paint annotation is stored in user annotation 0.

=a PaintTee */

class Paint : public Element {
  
  unsigned char _color;
  
 public:
  
  Paint();
  ~Paint();
  
  const char *class_name() const		{ return "Paint"; }
  const char *processing() const		{ return AGNOSTIC; }
  Paint *clone() const;
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  int color() const				{ return _color; }
  void set_color(unsigned char in_color)	{ _color = in_color; }
  
  Packet *simple_action(Packet *);

  void add_handlers();
  static String color_read_handler(Element *, void *);
  
};

CLICK_ENDDECLS
#endif
