// -*- c-basic-offset: 2; related-file-name: "../../lib/elemfilter.cc" -*-
#ifndef CLICK_ELEMFILTER_HH
#define CLICK_ELEMFILTER_HH
#include <click/element.hh>

class ElementFilter { public:
  
  ElementFilter()			{ }
  virtual ~ElementFilter()		{ }
  
  virtual bool match(Element *)		{ return false; }
  
  void filter(Vector<Element *> &);
  
};

class CastElementFilter : public ElementFilter { public:
  
  CastElementFilter(const String &);
  bool match(Element *);
  
 private:
  
  String _what;
  
};

#endif
