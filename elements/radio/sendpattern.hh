#ifndef CLICK_SENDPATTERN_HH
#define CLICK_SENDPATTERN_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * SendPattern(len)
 *
 * Keep sending packets of length len filled with a
 * repeating pattern. The point is to help diagnose
 * bit errors on a radio link.
 *
 * Meant to be used with CheckPattern.
 */

class SendPattern : public Element {
  
  int _len;
    
 public:
  
  SendPattern();
  ~SendPattern();
  
  const char *class_name() const		{ return "SendPattern"; }
  const char *processing() const	{ return PULL; }
  
  SendPattern *clone() const;
  int configure(Vector<String> &, ErrorHandler *);

  Packet *pull(int);
  
};

CLICK_ENDDECLS
#endif
