#ifndef PCT_HH
#define PCT_HH

#include <click/element.hh>

CLICK_DECLS

class Pct : public Element  {
  public:

  Pct();
  ~Pct();

  const char *class_name() const		{ return "Pct"; }
  const char *processing() const		{ return AGNOSTIC; }
  Pct *clone() const                           { return new Pct; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
};

CLICK_ENDDECLS
#endif
