#ifndef SETTIMESTAMP_HH
#define SETTIMESTAMP_HH

#include <click/element.hh>
#include <click/string.hh>

/*
 * =c
 * SetTimestamp([SECS, USECS])
 *
 * store the time in the packet's timestamp annotation
 * =d
 *
 * Store the specified time in the packet's timestamp annotation.  If
 * SECS and USECS are not specified, then set the annotation to the
 * current system time.
 * 
 * =a PrintOld */

class SetTimestamp : public Element { public:
  
  SetTimestamp();
  ~SetTimestamp();
  
  const char *class_name() const		{ return "SetTimestamp"; }
  void *cast(const char *);
  const char *processing() const		{ return AGNOSTIC; }
  SetTimestamp *clone() const;

  int configure(const Vector<String> &, ErrorHandler *);
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

 private:
  
  long _secs;
  long _usecs;

};

#endif
