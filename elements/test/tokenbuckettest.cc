/*
 * tokenbuckettest.{cc,hh} -- regression test element for TokenBucket
 * Cliff Frey
 *
 * Copyright (c) 2010 Meraki, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "tokenbuckettest.hh"
#include <click/tokenbucket.hh>
#include <click/error.hh>
CLICK_DECLS

TokenBucketTest::TokenBucketTest()
{
}

TokenBucketTest::~TokenBucketTest()
{
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);

int
TokenBucketTest::initialize(ErrorHandler *errh)
{
    TokenBucket tb;
    tb.assign(1024, 2048);
    tb.set_full();
    tb.fill((click_jiffies_t)0);
    CHECK(tb.remove_if(1024));
    CHECK(tb.remove_if(1024));
    CHECK(!tb.remove_if(1024));
    tb.fill((click_jiffies_t)(CLICK_HZ * 0.99));
    CHECK(!tb.remove_if(1024));
    tb.fill((click_jiffies_t)CLICK_HZ);
    CHECK(tb.remove_if(1024));
    CHECK(!tb.remove_if(1024));

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(TokenBucketTest)
CLICK_ENDDECLS
