// -*- c-basic-offset: 4 -*-
#ifndef CLICK_BIGINTTEST_HH
#define CLICK_BIGINTTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

BigintTest()

=s test

Test multiple-precision multiply and divide with some simple tests.

=d

This element routes no packets and does all its work at initialization time.

*/

class BigintTest : public Element { public:

    BigintTest();

    const char *class_name() const		{ return "BigintTest"; }

    int initialize(ErrorHandler *);

};

CLICK_ENDDECLS
#endif
