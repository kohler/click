// -*- c-basic-offset: 4 -*-
/*
 * dequetest.{cc,hh} -- regression test element for Deque
 * Eddie Kohler
 *
 * Copyright (c) 2011 Eddie Kohler
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
#include "dequetest.hh"
#include <click/deque.hh>
#include <click/error.hh>
#if CLICK_USERLEVEL
# include <sys/time.h>
# include <sys/resource.h>
# include <unistd.h>
#endif
CLICK_DECLS

DequeTest::DequeTest()
{
}

DequeTest::~DequeTest()
{
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test %<%s%> failed", __FILE__, __LINE__, #x);

int
DequeTest::initialize(ErrorHandler *errh)
{
    Deque<int> v;
    v.push_back(0);
    v.push_back(1);
    v.push_back(2);
    v.push_back(4);
    CHECK(v.size() == 4);
    CHECK(v.size() <= v.capacity());
    CHECK(v[0] == 0);
    CHECK(v[1] == 1);
    CHECK(v[2] == 2);
    CHECK(v[3] == 4);

    Deque<int>::iterator i = v.insert(v.end() - 1, 3);
    CHECK(i - v.begin() == 3);
    CHECK(*i == 3);
    CHECK(v.size() == 5 && v[0] == 0 && v[1] == 1 && v[2] == 2 && v[3] == 3 && v[4] == 4);

    i = v.insert(v.end(), 5);
    CHECK(i == v.end() - 1);
    CHECK(*i == 5);
    CHECK(v.back() == 5);

    i = v.insert(v.begin(), -1);
    CHECK(v.size() == 7);
    CHECK(i == v.end() - 7);
    CHECK(*i == -1);

    i = v.erase(v.begin(), v.begin() + 2);
    CHECK(*i == 1);
    CHECK(v.size() == 5);
    CHECK(i[1] == 2);

    i = v.erase(v.end() - 2, v.end());
    CHECK(i == v.end());
    CHECK(i[-1] == 3);
    CHECK(v.size() == 3);

    i = v.erase(v.begin() + 1, v.end() - 1);
    CHECK(i == v.end() - 1);
    CHECK(*i == 3);
    CHECK(i[-1] == 1);

    v.clear();
    for (int i = 10; i >= 0; --i)
	v.push_front(i);
    CHECK(v[0] == 0);
    CHECK(v[1] == 1);
    CHECK(v[10] == 10);
    v.erase(v.begin() + 1, v.begin() + 3);
    CHECK(v[0] == 0);
    CHECK(v[1] == 3);
    CHECK(v[8] == 10);
    v.erase(v.begin() + 6, v.begin() + 8);
    CHECK(v[5] == 7);
    CHECK(v[6] == 10);
    CHECK(v.size() == 7);

    // test case found by Mathias Kurth
    Deque<int> v2;
    v.clear();
    CHECK(v.size() == 0);
    CHECK(v2.size() == 0);
    for (int i = 0; i < 10000; i++)
	v = v2;

    Deque<String> sd;
    for (int i = 10; i >= 0; --i)
	sd.push_front(String(i));
    CHECK(sd[0] == "0");
    CHECK(sd[1] == "1");
    CHECK(sd[10] == "10");
    sd.erase(sd.begin() + 1, sd.begin() + 3);
    CHECK(sd[0] == "0");
    CHECK(sd[1] == "3");
    CHECK(sd[8] == "10");
    sd.erase(sd.begin() + 6, sd.begin() + 8);
    CHECK(sd[5] == "7");
    CHECK(sd[6] == "10");
    CHECK(sd.size() == 7);
    while (sd.size() < sd.capacity())
	sd.push_back(String(sd.size()));
    CHECK(sd[0] == "0");
    sd.insert(sd.begin() + 5, sd[0]);
    CHECK(sd[5] == "0");
    CHECK(sd[0] == "0");

    errh->message("All tests pass!");
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DequeTest)
