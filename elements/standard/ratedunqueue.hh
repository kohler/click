#ifndef RATEDUNQUEUE_HH
#define RATEDUNQUEUE_HH
#include <click/element.hh>
#include <click/gaprate.hh>
#include <click/task.hh>

/*
 * =c
 * RatedUnqueue(RATE)
 * =s packet scheduling
 * pull-to-push converter
 * =d
 * 
 * Pulls packets at the given RATE in packets per second, and pushes them out
 * its single output.
 *
 * =a BandwidthRatedUnqueue, Unqueue, Shaper, RatedSplitter */

class RatedUnqueue : public Element { public:
  
  RatedUnqueue();
  ~RatedUnqueue();
  
  const char *class_name() const		{ return "RatedUnqueue"; }
  const char *processing() const		{ return PULL_TO_PUSH; }
  
  RatedUnqueue *clone() const			{ return new RatedUnqueue; }
  int configure(Vector<String> &, ErrorHandler *);
  void configuration(Vector<String> &) const;
  int initialize(ErrorHandler *);
  void add_handlers();
  
  void run_scheduled();

  unsigned rate() const				{ return _rate.rate(); }
  void set_rate(unsigned, ErrorHandler * = 0);
  
 protected:

  GapRate _rate;
  Task _task;
  
};

#endif
