#ifndef PULLTOPUSH_HH
#define PULLTOPUSH_HH
#include "element.hh"

/*
 * =c
 * PullToPush([BURSTSIZE])
 * =s
 * old name for Unqueue
 * V<packet scheduling>
 * =d
 * This is the old name for the Unqueue element. You should use Unqueue
 * instead.
 * =a Unqueue */

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
