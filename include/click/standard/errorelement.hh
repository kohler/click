// -*- c-basic-offset: 4; related-file-name: "../../../elements/standard/errorelement.cc" -*-
#ifndef CLICK_ERRORELEMENT_HH
#define CLICK_ERRORELEMENT_HH
#include <click/element.hh>

/*
 * =c
 * Error(...)
 * =s debugging
 * always fails
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
  const char *flow_code() const			{ return "x/y"; }
  void notify_ninputs(int);
  void notify_noutputs(int);
  
  ErrorElement *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
};

#endif
