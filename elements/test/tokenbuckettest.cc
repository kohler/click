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

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);

int
TokenBucketTest::initialize(ErrorHandler *errh)
{
    TokenBucket tb;
    tb.assign(1024, 2048);
    CHECK(tb.rate() >= 1022 && tb.rate() <= 1026);
    CHECK(tb.capacity() >= 2046 && tb.capacity() <= 2050);
    tb.set_full();
    tb.refill((click_jiffies_t)0);
    CHECK(tb.remove_if(1024));
    CHECK(tb.remove_if(1024));
    CHECK(!tb.remove_if(1024));
    tb.refill((click_jiffies_t)CLICK_HZ * 99 / 100);
    CHECK(!tb.remove_if(1024));
    tb.refill((click_jiffies_t)CLICK_HZ);
    CHECK(tb.remove_if(1024));
    CHECK(!tb.remove_if(1024));

    TokenBucket tb2(1024*1024*1024, 1); // will change capacity
    CHECK(tb2.full());
    CHECK(tb2.remove_if(1000));

    TokenBucket tb3(1, 1024*1024*1024);
    CHECK(tb3.remove_if(2*(UINT_MAX/CLICK_HZ)));

    TokenBucket tb4(2, 1); // rate ends up very slightly less than 2
    tb4.refill(0);
    tb4.clear();
    tb4.refill(tb4.time_until_contains(1));
    CHECK(tb4.contains(1));

    tb4.refill(0);
    tb4.clear();
    click_jiffies_t done_at = tb4.time_until_contains(1);
    click_jiffies_t cur_time = 0;
    while (cur_time < done_at) {
        CHECK(cur_time == done_at - 1 || !tb4.contains(1));
        CHECK(tb4.time_until_contains(1) <= (TokenBucket::ticks_type) (done_at - cur_time));
        ++cur_time;
        tb4.refill(cur_time);
    }
    CHECK(tb4.contains(1));

    tb4.assign(true);
    CHECK(tb4.rate() == TokenBucket::max_tokens);
    CHECK(tb4.capacity() == TokenBucket::max_tokens);

    tb4.assign(false);
    tb4.clear();
    tb4.refill();
    CHECK(tb4.rate() == 0);
    CHECK(tb4.capacity() == TokenBucket::max_tokens);
    CHECK(tb4.empty());

    tb4.assign(0, 1024);
    CHECK(tb4.rate() == 0);
    CHECK(tb4.capacity() == 1024);

    tb4.assign(false);
    tb4.clear();
    tb4.refill();
    CHECK(tb4.rate() == 0);
    CHECK(tb4.capacity() == TokenBucket::max_tokens);
    CHECK(tb4.empty());

    tb4.assign(0, 1024);
    CHECK(tb4.rate() == 0);
    CHECK(tb4.capacity() == 1024);

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(TokenBucketTest)
CLICK_ENDDECLS
