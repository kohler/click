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
 private:
  int first_time_schedule;
 public:
  
  PullToPush() : Element(1,1) { first_time_schedule = 1; }
  
  const char *class_name() const		{ return "PullToPush"; }
  Processing default_processing() const		{ return PULL_TO_PUSH; }
  
  PullToPush *clone() const			{ return new PullToPush; }
  
  bool wants_packet_upstream() const;
  void run_scheduled();

#ifdef __KERNEL__
  /* hack - I can't find another place to do this scheduling */
  virtual struct wait_queue** get_wait_queue() 
  { 
      if (first_time_schedule) 
      {
	  schedule_tail(); 
	  first_time_schedule = 0;
      }
      return 0L; 
  }
#endif
};

#endif
