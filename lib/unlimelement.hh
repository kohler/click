#ifndef UNLIMELEMENT_HH
#define UNLIMELEMENT_HH
#include "element.hh"

class UnlimitedElement : public Element {

 public:
  
  UnlimitedElement();
  UnlimitedElement(int, int);
  
  virtual bool unlimited_inputs() const		{ return false; }
  virtual bool unlimited_outputs() const	{ return false; }
  
  void notify_ninputs(int);
  void notify_noutputs(int);

};

#endif
