#ifndef PULLTOPUSH_HH
#define PULLTOPUSH_HH
#include "element.hh"

/*
 * =c
 * PullToPush()
 * =d
 * Pulls packets whenever they are available, then pushes them out
 * its single output.
 */

class PullToPush : public Element {
  
 public:
  
  PullToPush();
  
  const char *class_name() const		{ return "PullToPush"; }
  Processing default_processing() const	{ return PULL_TO_PUSH; }
  
  PullToPush *clone() const			{ return new PullToPush; }
  
  bool wants_packet_upstream() const;
  void run_scheduled();
  
};

#endif
