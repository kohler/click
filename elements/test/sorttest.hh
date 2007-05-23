// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SORTTEST_HH
#define CLICK_SORTTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SortTest()

=s test

runs regression tests for click_qsort

=d

SortTest runs click_qsort regression tests at initialization time. It
does not route packets.

*/

class SortTest : public Element { public:

    SortTest();
    ~SortTest();

    const char *class_name() const		{ return "SortTest"; }

    int initialize(ErrorHandler *);

};

CLICK_ENDDECLS
#endif
