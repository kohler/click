// -*- c-basic-offset: 4 -*-
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
 * Statically binds elements to threads. If more than one StaticThreadSched
 * is specified, they will all run. The one that runs later may override an
 * earlier run.
 * =a
 * ThreadMonitor, BalancedThreadSched
 */

#include <click/element.hh>
#include <click/standard/threadsched.hh>

class StaticThreadSched : public Element, public ThreadSched { public:

    StaticThreadSched();
    ~StaticThreadSched();
  
    const char *class_name() const	{ return "StaticThreadSched"; }
  
    int configure(Vector<String> &, ErrorHandler *);

    int initial_thread_preference(Task *, bool);
    
  private:

    Vector<int> _thread_preferences;
    ThreadSched *_next_thread_sched;

};

#endif
