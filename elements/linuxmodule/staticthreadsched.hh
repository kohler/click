#ifndef STATICTHREADSCHED_HH
#define STATICTHREADSCHED_HH

/*
 * =c
 * StaticThreadSched(ELEMENT THREAD, ...)
 * =s information
 * specifies element and thread scheduling parameters
 * =io
 * None
 * =d
 * Each StaticThreadSched runs once and then removes itself from the worklist.
 * It statically binds elements to threads. If more than one StaticThreadSched
 * is specified, they will all run. The one that runs later may override an
 * earlier run.
 */

#include <click/timer.hh>
#include <click/element.hh>

class StaticThreadSched : public Element {

  Vector<String> _element_names;
  Vector<int> _threads;
  Timer _timer;

 public:

  StaticThreadSched();
  ~StaticThreadSched();
  
  const char *class_name() const	{ return "StaticThreadSched"; }
  
  StaticThreadSched *clone() const	{ return new StaticThreadSched; }
  int configure(const Vector<String> &, ErrorHandler *);

  int initialize(ErrorHandler *);
  void run_scheduled();
};

#endif
