// -*- c-basic-offset: 4 -*-
#ifndef CLICK_CRYPTOTEST_HH
#define CLICK_CRYPTOTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

CryptoTest()

=s test

runs regression tests for cryptography functions

=d

CryptoTest runs regression tests for Click's cryptography functions at
initialization time. It does not route packets.

*/

class CryptoTest : public Element { public:

    CryptoTest() CLICK_COLD;

    const char *class_name() const		{ return "CryptoTest"; }

    int initialize(ErrorHandler *errh) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
