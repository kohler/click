#ifndef CLICK_QUITWATCHER_HH
#define CLICK_QUITWATCHER_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

QuitWatcher(ELEMENT, ...)

=s debugging

stops router processing

=io

None

=d

Stops router processing when at least one of the ELEMENTs is no longer
scheduled.

=n

This element is inserted automatically by the user-level driver when
it is given the `--stop' option. */

class QuitWatcher : public Element { public:

  QuitWatcher();
  ~QuitWatcher();
  
  const char *class_name() const		{ return "QuitWatcher"; }
  QuitWatcher *clone() const			{ return new QuitWatcher; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void run_timer();

 private:
    
  Vector<Element *> _e;
  Vector<int> _handlers;
  Timer _timer;
  
};

CLICK_ENDDECLS
#endif
