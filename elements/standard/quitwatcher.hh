#ifndef QUITWATCHER_HH
#define QUITWATCHER_HH

/*
 * =c
 * QuitWatcher(ELEMENT, ...)
 * =io
 * None
 * =d
 * Stops router processing when not all the ELEMENTs are scheduled.
 * =n
 * This element is inserted automatically by the user-level driver when
 * it is given the `--stop' option.
 */

#include "element.hh"

class QuitWatcher : public Element {

  Vector<Element *> _e;
  
 public:
  
  QuitWatcher();
  
  const char *class_name() const		{ return "QuitWatcher"; }
  QuitWatcher *clone() const			{ return new QuitWatcher; }
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  
};

#endif
