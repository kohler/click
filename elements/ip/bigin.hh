#ifndef BIGIN_HH
#define BIGIN_HH

/*
 * =c
 * BigIn(color)
 * =d
 * Paint(color) -> Strip(14) -> CheckIPChecksum -> GetIPAddress(16)
 * =a Paint
 * =a CheckIPChecksum
 * =a Strip
 * =a GetIPAddress
 */

#include "element.hh"
#include "glue.hh"

class BigIn : public Element {
public:
  BigIn();
  ~BigIn();
  
  const char *class_name() const		{ return "BigIn"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  int drops() { return(_drops); }
  BigIn *clone() const;
  void add_handlers(HandlerRegistry *fcr);
  int configure(const String &, Router *, ErrorHandler *);

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
private:

  int _drops;
  int _color;

  int _n_bad_src;
  u_int *_bad_src; // array of illegal IP src addresses.

};

#endif
