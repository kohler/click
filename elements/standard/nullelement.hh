#ifndef NULLELEMENT_HH
#define NULLELEMENT_HH
#include "element.hh"

/*
 * =c
 * Null()
 * =d
 * Just passes packets along without doing anything else.
 */

class NullElement : public Element {
  
 public:
  
  NullElement();
  
  const char *class_name() const		{ return "Null"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  NullElement *clone() const;
  
  Packet *simple_action(Packet *);
  
};

#endif
