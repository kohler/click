#ifndef NULLELEMENT_HH
#define NULLELEMENT_HH
#include <click/element.hh>

/*
 * =c
 * Null
 * =s
 * passes packets unchanged
 * =d
 * Just passes packets along without doing anything else.
 */

class NullElement : public Element { public:
  
  NullElement();
  ~NullElement();
  
  const char *class_name() const	{ return "Null"; }
  const char *processing() const	{ return AGNOSTIC; }
  NullElement *clone() const;
  
  Packet *simple_action(Packet *);
  
};

#endif
