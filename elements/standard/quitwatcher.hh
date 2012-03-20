#ifndef CLICK_QUITWATCHER_HH
#define CLICK_QUITWATCHER_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS
class Handler;

/*
=c

QuitWatcher(ELEMENT, ...)

=s control

stops router processing

=d

Stops router processing when at least one of the ELEMENTs is no longer
scheduled.

*/

class QuitWatcher : public Element { public:

  QuitWatcher();

  const char *class_name() const		{ return "QuitWatcher"; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void run_timer(Timer *);

 private:

  Vector<Element*> _e;
  Vector<const Handler*> _handlers;
  Timer _timer;

};

CLICK_ENDDECLS
#endif
