#ifndef ELEMENTSCHED_HH
#define ELEMENTSCHED_HH

/*
 * =c
 * ElementSched(ELEMENT SCHEDULING_PARAM, ...)
 * =io
 * None
 * =d
 * Grab scheduling parameters for specified element. Each configuration
 * argument has the form `ELEMENT SCHEDULING_PARAM', meaning that the element
 * named ELEMENT has the scheduling parameter SCHEDULING_PARAM.
 */

#include "element.hh"

class ElementSched : public Element {

 public:
  
  ElementSched();
  
  const char *class_name() const		{ return "ElementSched"; }
  ElementSched *clone() const			{ return new ElementSched; }

  int configure(const String &, ErrorHandler *);
};

#endif
