#ifndef ERRORELEMENT_HH
#define ERRORELEMENT_HH
#include "unlimelement.hh"

class ErrorElement : public UnlimitedElement {
  
 public:
  
  ErrorElement();
  
  const char *class_name() const		{ return "Error"; }
  Processing default_processing() const		{ return AGNOSTIC; }

  bool unlimited_inputs() const			{ return true; }
  bool unlimited_outputs() const		{ return true; }
  
  ErrorElement *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  int initialize(Router *, ErrorHandler *);
  void add_handlers(HandlerRegistry *);
  
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  
};

#endif
