#ifndef BIGIN_HH
#define BIGIN_HH

/*
 * =c
 * BigIn(color)
 * =d
 * Paint(color) -> Strip(14) -> CheckIPHeader -> GetIPAddress(16)
 * =a Paint
 * =a CheckIPHeader
 * =a Strip
 * =a GetIPAddress
 */

#include "element.hh"
#include "glue.hh"

class BigIn : public Element {
  
  int _drops;
  int _color;

  int _n_bad_src;
  u_int *_bad_src; // array of illegal IP src addresses.

 public:
  
  BigIn();
  ~BigIn();
  
  const char *class_name() const		{ return "BigIn"; }
  Processing default_processing() const		{ return AGNOSTIC; }
  
  int drops() const				{ return(_drops); }
  BigIn *clone() const;
  void add_handlers(HandlerRegistry *fcr);
  int configure(const String &, ErrorHandler *);

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

#endif
