#ifndef ERRORELEMENT_HH
#define ERRORELEMENT_HH
#include <click/element.hh>

/*
 * =c
 * Error(...)
 * =s always fails
 * =d
 * The Error element always fails to initialize. It has any number of inputs
 * and outputs, and accepts any configuration string without complaint. It is
 * useful to prevent a router from initializing while avoiding
 * spurious error messages about bad configuration strings or connections.
 */

class ErrorElement : public Element { public:
  
  ErrorElement();
  ~ErrorElement();
  
  const char *class_name() const		{ return "Error"; }
  const char *processing() const		{ return AGNOSTIC; }
  void notify_ninputs(int);
  void notify_noutputs(int);
  
  ErrorElement *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  
};

#endif
