#ifndef ANNOTATE_HH
#define ANNOTATE_HH
#include "element.hh"
#include "ipaddress.hh"

/*
 * =c
 * Annotate()
 * =d
 * Expects IP packets. Copies the IP header's TOS, TTL,
 * and offset fields into the Click packet annotation.
 *
 * These annotations are used by the IPEncap element.
 *
 * =a IPEncap
 */

class Annotate : public Element {
  
  int _offset;
  
 public:
  
  Annotate();
  
  const char *class_name() const		{ return "Annotate"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  Annotate *clone() const;
  
  Packet *simple_action(Packet *);
  
};

#endif
