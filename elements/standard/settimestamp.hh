// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SETTIMESTAMP_HH
#define CLICK_SETTIMESTAMP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetTimestamp([TIMESTAMP, I<keyword> FIRST, DELTA])

=s annotations

store the time in the packet's timestamp annotation

=d

Store the specified TIMESTAMP in the packet's timestamp annotation. If
TIMESTAMP is not specified, then sets the annotation to the system time when
the packet arrived at the SetTimestamp element.

Keyword arguments are:

=over 8

=item FIRST

Boolean.  If true, then set the packet's "first timestamp" annotation, not its
timestamp annotation.  Default is true.

=item DELTA

Boolean.  If true, then set the packet's timestamp annotation to the
difference between its current timestamp annotation and its "first timestamp"
annotation.  Default is false.

=back

=a StoreTimestamp, PrintOld */

class SetTimestamp : public Element { public:
  
    SetTimestamp();
    ~SetTimestamp();
  
    const char *class_name() const		{ return "SetTimestamp"; }
    const char *processing() const		{ return AGNOSTIC; }
    int configure(Vector<String> &, ErrorHandler *);
  
    Packet *simple_action(Packet *);

  private:

    enum { ACT_NOW, ACT_TIME, ACT_FIRST_NOW, ACT_FIRST_TIME, ACT_DELTA };
    int _action;
    Timestamp _tv;

};

CLICK_ENDDECLS
#endif
