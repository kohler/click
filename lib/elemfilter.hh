#ifndef ELEMFILTER_HH
#define ELEMFILTER_HH
#include "element.hh"

class ElementFilter { public:
  
  ElementFilter()			{ }
  virtual ~ElementFilter()		{ }
  
  virtual bool match(Element *)		{ return false; }
  
  void filter(Vector<Element *> &);
  
};

class CastElementFilter : public ElementFilter {
  
  String _what;
  
 public:
  
  CastElementFilter(const String &);
  bool match(Element *);
  
};

#endif
