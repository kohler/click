// -*- c-basic-offset: 4 -*-
/*
 * biginttest.{cc,hh} -- regression test element for Bigint
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
 * Copyright (c) 2011 Regents of the University of California
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
#include "biginttest.hh"
#include <click/hashtable.hh>
#include <click/error.hh>
#include <click/bigint.hh>
#if CLICK_USERLEVEL
# include <sys/time.h>
# include <sys/resource.h>
# include <unistd.h>
#endif
CLICK_DECLS

BigintTest::BigintTest()
{
}

#define CHECK(x, a, b) if (!(x)) return errh->error("%s:%d: test `%s' failed [%llu, %u]", __FILE__, __LINE__, #x, a, b);
#define CHECK0(x) if (!(x)) return errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);

static bool test_multiply(uint32_t a, uint32_t b, ErrorHandler *errh) {
    uint32_t x[2];
    bigint::multiply(x[1], x[0], a, b);
    uint64_t c = (((uint64_t) x[1]) << 32) | x[0];
    if (c != (uint64_t) a * b) {
        errh->error("%u * %u == %llu, not %llu", a, b, (uint64_t) a * b, c);
        return false;
    }
    return true;
}

static bool test_mul(uint64_t a, uint32_t b, ErrorHandler *errh) {
    uint32_t ax[2];
    ax[0] = a;
    ax[1] = a >> 32;
    uint32_t cx[2];
    cx[0] = cx[1] = 0;
    bigint::multiply_add(cx, ax, 2, b);
    uint64_t c = (((uint64_t) cx[1]) << 32) | cx[0];
    if (c != a * b) {
        errh->error("%llu * %u == %llu, not %llu", a, b, a * b, c);
        return false;
    }
    return true;
}

static bool test_div(uint64_t a, uint32_t b, ErrorHandler *errh) {
    uint32_t ax[4];
    ax[0] = a;
    ax[1] = a >> 32;
    uint32_t r = bigint::divide(ax+2, ax, 2, b);
    uint64_t c = ((uint64_t) ax[3] << 32) | ax[2];
    if (c != a / b) {
        errh->error("%llu / %u == %llu, not %llu", a, b, a * b, c);
        return false;
    }
    if (r != a % b) {
        errh->error("%llu %% %u == %llu, not %u", a, b, a % b, r);
        return false;
    }
    return true;
}

static bool test_inverse(uint32_t a, ErrorHandler *errh) {
    assert(a & (1 << 31));
    uint32_t a_inverse = bigint::inverse(a);
    // "Inverse is floor((b * (b - a) - 1) / a), where b = 2^32."
    uint64_t b = (uint64_t) 1 << 32;
    uint64_t want_inverse = (b * (b - a) - 1) / a;
    assert(want_inverse < b);
    if (a_inverse != want_inverse) {
        errh->error("inverse(%u) == %u, not %u", a, (uint32_t) want_inverse, a_inverse);
        return false;
    }
    return true;
}

static bool test_add(uint64_t a, uint64_t b, ErrorHandler *errh) {
    uint32_t ax[6];
    ax[2] = a;
    ax[3] = a >> 32;
    ax[4] = b;
    ax[5] = b >> 32;
    bigint::add(ax[1], ax[0], ax[3], ax[2], ax[5], ax[4]);
    uint64_t c = ((uint64_t) ax[1] << 32) | ax[0];
    if (c != a + b) {
        errh->error("%llu + %llu == %llu, not %llu", a, b, a + b, c);
        return false;
    }
    return true;
}

int
BigintTest::initialize(ErrorHandler *errh)
{
    for (int i = 0; i < 3000; i++) {
        uint32_t a = click_random() | (click_random() << 31);
        uint32_t b = click_random() | (click_random() << 31);
        CHECK(test_multiply(a, b, errh), a, b);
        CHECK(test_mul(a, b, errh), a, b);
    }
    for (int i = 0; i < 8000; i++) {
        uint32_t a = click_random();
        CHECK0(test_inverse(a | 0x80000000, errh));
    }
    CHECK0(test_inverse(0x80000000, errh));
    for (int i = 0; i < 8000; i++) {
        uint64_t a = click_random() | ((uint64_t) click_random() << 31) | ((uint64_t) click_random() << 62);
        uint64_t b = click_random() | ((uint64_t) click_random() << 31) | ((uint64_t) click_random() << 62);
        CHECK0(test_add(a, b, errh));
    }
    CHECK0(test_div(12884758640815563913ULL, 2506284098U, errh));
    for (int i = 0; i < 3000; i++) {
        uint64_t a = click_random() | ((uint64_t) click_random() << 31) | ((uint64_t) click_random() << 62);
        uint32_t b = click_random();
        CHECK(test_div(a, b | 0x80000000, errh), a, b | 0x80000000);
    }
    for (int i = 0; i < 3000; i++) {
        uint64_t a = click_random() | ((uint64_t) click_random() << 31) | ((uint64_t) click_random() << 62);
        uint32_t b = click_random();
        CHECK(test_div(a, b & ~0x80000000, errh), a, b & ~0x80000000);
        CHECK(test_div(a, b | 0x80000000, errh), a, b | 0x80000000);
    }

    uint32_t x[3] = { 3481, 592182, 3024921038U };
    CHECK0(bigint::unparse_clear(x, 3) == "55799944231168388787108580761");

    x[0] = 10;
    x[1] = 0;
    CHECK0(bigint::multiply(x, x, 2, 10) == 0 && x[0] == 100 && x[1] == 0);
    CHECK0(bigint::multiply(x, x, 2, 4191384139U) == 0 && x[0] == 0x9698A54CU && x[1] == 0x61U);

    {
	int32_t quot, rem;
	rem = int_divide((int32_t) 0x80000000, 2, quot);
	CHECK0(quot == -0x40000000 && rem == 0);
	rem = int_divide((int32_t) 0x80000000, 3, quot);
	CHECK0(quot == -715827883 && rem == 1);
    }

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(BigintTest)
ELEMENT_REQUIRES(userlevel int64)
CLICK_ENDDECLS
