#ifndef READHANDLER_HH
#define READHANDLER_HH

#include "element.hh"

/*
 * =c
 * ReadHandlerCaller()
 * =d
 * Dumps the output of read handlers by calling a routine in the userlevel
 * driver. This element is needed because we want to use Click's scheduling
 * mechanism to schedule when read handlers are called.
 */

class ReadHandlerCaller : public Element {
  
 public:
  
  ReadHandlerCaller() {}
  ~ReadHandlerCaller() {}

  const char *class_name() const	{ return "ReadHandlerCaller"; }
  const char *processing() const	{ return AGNOSTIC; }
  ReadHandlerCaller *clone() const	{ return new ReadHandlerCaller; }
  
  void run_scheduled();
};

#endif
