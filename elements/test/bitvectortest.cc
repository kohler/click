// -*- c-basic-offset: 4 -*-
/*
 * bitvectortest.{cc,hh} -- regression test element for Bitvector
 * Eddie Kohler
 *
 * Copyright (c) 2012 Eddie Kohler
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
#include "bitvectortest.hh"
#include <click/bitvector.hh>
#include <click/error.hh>
CLICK_DECLS

BitvectorTest::BitvectorTest()
{
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test %<%s%> failed", __FILE__, __LINE__, #x);
#define CHECK_DATA(x, y, l) CHECK(memcmp((x), (y), (l)) == 0)

int
BitvectorTest::initialize(ErrorHandler *errh)
{
    Bitvector bv;
    CHECK(bv.size() == 0);
    CHECK(bv.zero());
    CHECK(!bv);

    bv.resize(40);
    bv[39] = true;
    CHECK(bv.size() == 40);
    CHECK(bv[39]);
    CHECK(bv);

    bv.resize(10);
    CHECK(!bv);

    bv.resize(40);
    CHECK(!bv);
    CHECK(!bv[39]);

    bv[0] = true;
    bv.resize(0);
    CHECK(bv.words()[0] == 0);

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(BitvectorTest)
CLICK_ENDDECLS
