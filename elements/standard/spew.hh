#ifndef SPEW_HH
#define SPEW_HH
#include "element.hh"
#include "timer.hh"

/*
 * Spew(n)
 * Generate n garbage packets.
 * The point is to help benchmark.
 */

class Spew : public Element {
  
  Timer _timer;
  int _n;
  bool _quit;
  int _done;
    
 public:
  
  Spew();
  
  const char *class_name() const		{ return "Spew"; }
  const char *processing() const	{ return PUSH; }
  
  Spew *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();
  void spew_some();
  
};

#endif
