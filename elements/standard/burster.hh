#ifndef BURSTER_HH
#define BURSTER_HH
#include <click/element.hh>
#include <click/timer.hh>

/*
 * =c
 * Burster(I, N)
 * =s pull-to-push converter
 * =d
 * Pulls N packets each interval I (seconds) from its input.
 * Pushes them out its single output. The interval can be
 * a floating point number.
 *
 * There are usually Queues both upstream and downstream
 * of Burster elements.
 * 
 * =n
 * The UNIX and Linux timers have granularity of about 10
 * milliseconds, so this Burster can only produce high packet
 * rates by being bursty.
 */

class Burster : public Element {
  
  int _npackets;
  Timer _timer;
  int _interval;
  
 public:
  
  Burster();
  ~Burster();
  
  const char *class_name() const                { return "Burster"; }
  const char *processing() const       { return PULL_TO_PUSH; }
  
  Burster *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  
};

#endif
