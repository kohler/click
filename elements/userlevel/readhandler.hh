#ifndef READHANDLER_HH
#define READHANDLER_HH

#include <click/element.hh>

/*
 * =c
 * ReadHandlerCaller(n)
 * =s
 * V<debugging>
 * =d
 * Dumps the output of read handlers by calling a routine in the userlevel
 * driver. Run roughly every n seconds. If delayed, will not try to catch up,
 * but rather will run once and wait for n seconds again. This element is
 * needed because we want to use Click's scheduling mechanism to schedule when
 * read handlers are called.
 */

class ReadHandlerCaller : public Element {
  unsigned _n;
  unsigned _last_time;
  
 public:
  
  ReadHandlerCaller() {}
  ~ReadHandlerCaller() {}

  const char *class_name() const	{ return "ReadHandlerCaller"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(const Vector<String> &conf, ErrorHandler *errh);
  int initialize(ErrorHandler *);
  
  ReadHandlerCaller *clone() const	{ return new ReadHandlerCaller; }
  void run_scheduled();
};

#endif
