#ifndef PULLTOPUSH_HH
#define PULLTOPUSH_HH
#include "element.hh"

/*
 * =c
 * PullToPush([BURSTSIZE])
 * =s pull-to-push converter
 * =d
 * Pulls packets whenever they are available, then pushes them out
 * its single output. Pulls a maximum of BURSTSIZE packets every time
 * it is scheduled. Default BURSTSIZE is 1.
 */

class PullToPush : public Element {

  int _burst;

 public:
  
  PullToPush() : Element(1, 1)			{ }
  
  const char *class_name() const		{ return "PullToPush"; }
  const char *processing() const		{ return PULL_TO_PUSH; }
  
  PullToPush *clone() const			{ return new PullToPush; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();

};

#endif
