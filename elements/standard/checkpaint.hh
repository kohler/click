#ifndef CLICK_CHECKPAINT_HH
#define CLICK_CHECKPAINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

CheckPaint(COLOR)

=s checking

checks packets' paint annotation

=d

Checks that incoming packets have paint annotation equal to COLOR. If their
paints are not equal to COLOR, then they are dropped or emitted on output 1,
depending on how many outputs were used.

=a Paint, PaintTee */

class CheckPaint : public Element { public:

  CheckPaint();
  ~CheckPaint();
  
  const char *class_name() const	{ return "CheckPaint"; }
  const char *processing() const	{ return "a/ah"; }
  CheckPaint *clone() const		{ return new CheckPaint; }

  void notify_noutputs(int);
  int configure(Vector<String> &, ErrorHandler *);
  
  void push(int, Packet *);
  Packet *pull(int);
  
 private:
  
  uint8_t _color;
  
};

CLICK_ENDDECLS
#endif
