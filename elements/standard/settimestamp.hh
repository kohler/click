#ifndef SETTIMESTAMP_HH
#define SETTIMESTAMP_HH
#include <click/element.hh>

/*
=c

SetTimestamp([TIMESTAMP])

=s annotations

store the time in the packet's timestamp annotation

=d

Store the specified TIMESTAMP in the packet's timestamp annotation. If
TIMESTAMP is not specified, then sets the annotation to the system time when
the packet arrived at the SetTimestamp element.

=a PrintOld */

class SetTimestamp : public Element { public:
  
  SetTimestamp();
  ~SetTimestamp();
  
  const char *class_name() const		{ return "SetTimestamp"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetTimestamp *clone() const;

  int configure(const Vector<String> &, ErrorHandler *);
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

 private:

  struct timeval _tv;

};

#endif
