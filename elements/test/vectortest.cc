// -*- c-basic-offset: 4 -*-
/*
 * vectortest.{cc,hh} -- regression test element for Vector
 * Eddie Kohler
 *
 * Copyright (c) 2004 Regents of the University of California
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
#include "vectortest.hh"
#include <click/vector.hh>
#include <click/error.hh>
#if CLICK_USERLEVEL
# include <sys/time.h>
# include <sys/resource.h>
# include <unistd.h>
#endif
CLICK_DECLS

VectorTest::VectorTest()
{
    MOD_INC_USE_COUNT;
}

VectorTest::~VectorTest()
{
    MOD_DEC_USE_COUNT;
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);

int
VectorTest::initialize(ErrorHandler *errh)
{
    Vector<int> v;
    v.push_back(0);
    v.push_back(1);
    v.push_back(2);
    v.push_back(4);
    CHECK(v.size() == 4);
    
    Vector<int>::iterator i = v.insert(v.end() - 1, 3);
    CHECK(i - v.begin() == 3);
    CHECK(*i == 3);
    CHECK(v.size() == 5 && v[0] == 0 && v[1] == 1 && v[2] == 2 && v[4] == 4);

    i = v.insert(v.end(), 5);
    CHECK(i == v.end() - 1);
    CHECK(*i == 5);
    CHECK(v.back() == 5);
    
    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(VectorTest)
#include <click/vector.cc>
CLICK_ENDDECLS
