#ifndef CHANGEUID_HH
#define CHANGEUID_HH

/*
 * =c
 * ChangeUID(UID, TIMEOUT)
 * =s
 * Changes UID of click process after startup. Waits <TIMEOUT> milliseconds.
 * =d
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/timer.hh>

class ChangeUID : public Element {

  unsigned int _uid, _timeout;
  Timer _timer;

 public:

  ChangeUID();
  ~ChangeUID();

  const char *class_name() const		{ return "ChangeUID"; }
  const char *port_count() const		{ return "0/0"; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  static void timer_hook(Timer *, void *);
};

#endif
