// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SETTIMESTAMP_HH
#define CLICK_SETTIMESTAMP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetTimestamp([TIMESTAMP])

=s annotations

store the time in the packet's timestamp annotation

=d

Store the specified TIMESTAMP in the packet's timestamp annotation. If
TIMESTAMP is not specified, then sets the annotation to the system time when
the packet arrived at the SetTimestamp element.

=a StoreTimestamp, PrintOld */

class SetTimestamp : public Element { public:
  
    SetTimestamp();
    ~SetTimestamp();
  
    const char *class_name() const		{ return "SetTimestamp"; }
    const char *processing() const		{ return AGNOSTIC; }
    int configure(Vector<String> &, ErrorHandler *);
  
    Packet *simple_action(Packet *);

  private:

    struct timeval _tv;

};

CLICK_ENDDECLS
#endif
