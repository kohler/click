#ifndef ERRORELEMENT_HH
#define ERRORELEMENT_HH
#include "unlimelement.hh"

/*
 * =c
 * Error(...)
 * =d
 * The Error element always fails to initialize. It has any number of inputs
 * and outputs, and accepts any configuration string without complaint. It is
 * useful to prevent a router from initializing while avoiding
 * spurious error messages.
 */

class ErrorElement : public UnlimitedElement {
  
 public:
  
  ErrorElement();
  
  const char *class_name() const		{ return "Error"; }
  const char *processing() const		{ return AGNOSTIC; }

  bool unlimited_inputs() const			{ return true; }
  bool unlimited_outputs() const		{ return true; }
  
  ErrorElement *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  
};

#endif
