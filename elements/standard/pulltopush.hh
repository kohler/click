#ifndef PULLTOPUSH_HH
#define PULLTOPUSH_HH
#include <click/element.hh>

/*
 * =c
 * PullToPush([BURSTSIZE])
 * =s packet scheduling
 * old name for Unqueue
 * =d
 * This is the old name for the Unqueue element. You should use Unqueue
 * instead.
 * =a Unqueue */

class PullToPush : public Element {

  int _burst;

 public:
  
  PullToPush();
  ~PullToPush();
  
  const char *class_name() const		{ return "PullToPush"; }
  const char *processing() const		{ return PULL_TO_PUSH; }
  
  PullToPush *clone() const			{ return new PullToPush; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();

};

#endif
