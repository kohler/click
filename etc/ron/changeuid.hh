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
#include <click/click_ip.h>
#include <click/timer.hh>

class ChangeUID : public Element {
  
  unsigned int _uid, _timeout;
  Timer _timer;

 public:
  
  ChangeUID();
  ~ChangeUID();
  
  const char *class_name() const		{ return "ChangeUID"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  ChangeUID *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  static void timer_hook(Timer *, void *);
};

#endif
