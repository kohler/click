#ifndef CLICK_LISTTEST_HH
#define CLICK_LISTTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

ListTest()

=s test

runs regression tests for List<T, link>

=d

ListTest runs List regression tests at initialization time. It does not route
packets.

*/

class ListTest : public Element { public:

    ListTest();

    const char *class_name() const		{ return "ListTest"; }

    int initialize(ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif
