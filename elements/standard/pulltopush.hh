#ifndef PULLTOPUSH_HH
#define PULLTOPUSH_HH
#include "element.hh"

/*
 * =c
 * PullToPush()
 * =d
 *
 * Pulls packets whenever they are available, then pushes them out
 * its single output. Always place itself on work list.
 */

class PullToPush : public Element {

 public:
  
  PullToPush() : Element(1, 1)			{ }
  
  const char *class_name() const		{ return "PullToPush"; }
  Processing default_processing() const		{ return PULL_TO_PUSH; }
  
  PullToPush *clone() const			{ return new PullToPush; }
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();

};

#endif
