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

    CHECK(!bv.parse("3", 0, 5, -3));
    CHECK(!bv.parse("3", 5, 3));

    CHECK(!bv.parse("-", 0, 5));
    CHECK(!bv.parse("--2", 0, 5));
    CHECK(!bv.parse("2-", 0, 5));
    CHECK(!bv.parse("2--3", -5, 5));
    CHECK(!bv.parse("3", 0, 2));
    CHECK(!bv.parse("1", 3, 5));
    CHECK(!bv.parse("1-4", 3, 5));
    CHECK(!bv.parse("4-6", 3, 5));

    CHECK(bv.parse("1,3,5", 0, 6));
    CHECK(bv.size() == 7 && !bv[0] && bv[1] && !bv[2] && bv[3] && !bv[4] && bv[5] && !bv[6]);

    CHECK(bv.parse("", 1, 3));
    CHECK(bv.size() == 3 && !bv[0] && !bv[1] && !bv[2]);

    CHECK(bv.parse("-1,1,3", -1, 3));
    CHECK(bv.size() == 5 && bv[0] && !bv[1] && bv[2] && !bv[3] && bv[4]);

    CHECK(bv.parse("-1-0,2-3,5-6", -2, 7));
    CHECK(bv.size() == 10 && !bv[0] && bv[1] && bv[2] && !bv[3] && bv[4] && bv[5] && !bv[6] && bv[7] && bv[8] && !bv[9]);

    CHECK(bv.parse("-7--6,-4--3,-1-0", -7, 0));
    CHECK(bv.size() == 8 && bv[0] && bv[1] && !bv[2] && bv[3] && bv[4] && !bv[5] && bv[6] && bv[7]);

    bv.assign(6, true);
    bv[2] = bv[4] = false;
    CHECK(bv.unparse() == "0-1,3,5");
    CHECK(bv.unparse(0) == "0-1,3,5");
    CHECK(bv.unparse(-1) == "1-2,4,6");
    CHECK(bv.unparse(1) == "0,2,4");
    CHECK(bv.unparse(8) == "");
    CHECK(bv.unparse(0, 1) == "1-2,4,6");
    CHECK(bv.unparse(0, -1) == "-1-0,2,4");
    CHECK(bv.unparse(1, 1) == "1,3,5");
    bv[4] = true;
    CHECK(bv.unparse() == "0-1,3-5");

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(BitvectorTest)
CLICK_ENDDECLS
