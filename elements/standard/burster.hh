#ifndef CLICK_BURSTER_HH
#define CLICK_BURSTER_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
 * =c
 * Burster(INTERVAL, BURST)
 * =s shaping
 * pull-to-push converter
 * =d
 * Pulls BURST packets each INTERVAL (seconds) from its input.
 * Pushes them out its single output. The interval can be
 * a floating point number.  Default BURST is 8.
 *
 * There are usually Queues both upstream and downstream
 * of Burster elements.
 *
 * =n
 * The UNIX and Linux timers have granularity of about 10
 * milliseconds, so this Burster can only produce high packet
 * rates by being bursty.
 */

class Burster : public Element { public:

  Burster();
  ~Burster();

  const char *class_name() const                { return "Burster"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const       { return PULL_TO_PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void run_timer(Timer *);

 private:

  int _npackets;
  Timer _timer;
  int _interval;

};

CLICK_ENDDECLS
#endif
