#ifndef UNLIMELEMENT_HH
#define UNLIMELEMENT_HH
#include "element.hh"

class UnlimitedElement : public Element {

 public:
  
  UnlimitedElement();
  UnlimitedElement(int, int);
  
  virtual bool unlimited_inputs() const		{ return false; }
  virtual bool unlimited_outputs() const	{ return false; }
  
  void notify_inputs(int);
  void notify_outputs(int);

};

#endif
