// -*- c-basic-offset: 4 -*-
/*
 * bhmtest.{cc,hh} -- regression test element for BigHashMap
 * Eddie Kohler
 *
 * Copyright (c) 2003 International Computer Science Institute
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
#include "bhmtest.hh"
#include <click/bighashmap.hh>
#include <click/error.hh>
CLICK_DECLS

BigHashMapTest::BigHashMapTest()
{
    MOD_INC_USE_COUNT;
}

BigHashMapTest::~BigHashMapTest()
{
    MOD_DEC_USE_COUNT;
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);
#define CHECK_DATA(x, y, l) CHECK(memcmp((x), (y), (l)) == 0)

static int
check1(BigHashMap<String, int> &h, ErrorHandler *errh)
{
    CHECK(h.size() == 4);
    CHECK(!h.empty());

    char x[4] = "\0\0\0";
    int n = 0;
    for (BigHashMap<String, int>::const_iterator i = h.begin(); i; i++) {
	CHECK(i.value() >= 1 && i.value() <= 4);
	CHECK(x[i.value() - 1] == 0);
	x[i.value() - 1] = 1;
	n++;
    }
    CHECK(n == 4);

    return 0;
}

int
BigHashMapTest::initialize(ErrorHandler *errh)
{
    BigHashMap<String, int> h(-1);

    h.insert("Foo", 1);
    h.insert("bar", 2);
    h.insert("facker", 3);
    h.insert("Anne Elizabeth Dudfield", 4);

    CHECK(check1(h, errh) == 0);

    // check copy constructor
    {
	BigHashMap<String, int> hh(h);
	CHECK(check1(hh, errh) == 0);
	hh.insert("crap", 5);
    }

    CHECK(check1(h, errh) == 0);

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(BigHashMapTest)

#include <click/bighashmap.cc>
CLICK_ENDDECLS
