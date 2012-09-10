#ifndef CLICK_NOTIFIERTEST_HH
#define CLICK_NOTIFIERTEST_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c
NotifierTest()

=s test
run regression tests for Notifier

=d
NotifierTest runs notification regression tests at initialization
time. It does not route packets.
*/

class NotifierTest : public Element { public:

    NotifierTest() CLICK_COLD;

    const char *class_name() const	{ return "NotifierTest"; }
    int initialize(ErrorHandler *errh) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
