#ifndef ELEMFILTER_HH
#define ELEMFILTER_HH
#include "element.hh"

class ElementFilter { public:
  
  ElementFilter()			{ }
  virtual ~ElementFilter()		{ }
  
  virtual bool match(Element *)		{ return false; }
  
  void filter(Vector<Element *> &);
  
};

class IsaElementFilter : public ElementFilter {
  
  String _what;
  
 public:
  
  IsaElementFilter(const String &);
  bool match(Element *);
  
};

class WantsPacketUpstreamElementFilter : public ElementFilter {

 public:
  
  WantsPacketUpstreamElementFilter()	{ }
  bool match(Element *);
  
};

#endif
