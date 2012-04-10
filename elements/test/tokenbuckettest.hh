// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TOKENBUCKETTEST_HH
#define CLICK_TOKENBUCKETTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TokenBucketTest()

=s test

runs regression tests for token bucket

=d

TokenBucketTest runs TokenBucket regression tests at initialization
time. It does not route packets.

*/

class TokenBucketTest : public Element { public:

    TokenBucketTest();

    const char *class_name() const		{ return "TokenBucketTest"; }

    int initialize(ErrorHandler *);

};

CLICK_ENDDECLS
#endif
