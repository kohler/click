// -*- c-basic-offset: 4 -*-
/*
 * heaptest.{cc,hh} -- regression test element for heap functions
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
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
#include "heaptest.hh"
#include <click/heap.hh>
#include <click/error.hh>
#include <click/pair.hh>
CLICK_DECLS

HeapTest::HeapTest()
{
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test %<%s%> failed", __FILE__, __LINE__, #x);
#define CHECK_PLACE(v) for (int iii = 0; iii < (v).size(); ++iii) if ((v)[iii].second != iii) return errh->error("%s:%d: place failed at position %d", __FILE__, __LINE__, iii);

namespace {
struct place_intpair {
    void operator()(Pair<int, int> *begin, Pair<int, int> *x) {
	x->second = x - begin;
    }
};
}

int
HeapTest::initialize(ErrorHandler *errh)
{
    Vector<int> v;
    less<int> l;

    v.push_back(0);
    push_heap(v.begin(), v.end(), l);

    v.push_back(-1);
    push_heap(v.begin(), v.end(), l);
    CHECK(v[0] == -1);
    CHECK(v[1] == 0);

    v.push_back(1);
    push_heap(v.begin(), v.end(), l);
    CHECK(v[0] == -1);
    CHECK(v[1] == 0);
    CHECK(v[2] == 1);

    pop_heap(v.begin(), v.end(), l);
    CHECK(v[0] == 0);
    CHECK(v[1] == 1);
    CHECK(v[2] == -1);
    v.pop_back();

    v.push_back(10);
    push_heap(v.begin(), v.end(), l);
    v.push_back(8);
    push_heap(v.begin(), v.end(), l);
    CHECK(v[0] == 0);
    CHECK(v[1] == 1);
    CHECK(v[2] == 10);
    CHECK(v[3] == 8);

    v[2] = -2;
    change_heap(v.begin(), v.end(), v.begin() + 2, l);
    CHECK(v[0] == -2);
    CHECK(v[1] == 1);
    CHECK(v[2] == 0);
    CHECK(v[3] == 8);

    pop_heap(v.begin(), v.end(), l);
    CHECK(v[0] == 0);
    CHECK(v[1] == 1);
    CHECK(v[2] == 8);
    CHECK(v[3] == -2);


    Vector<Pair<int, int> > vv;
    less<Pair<int, int> > ll;
    place_intpair place;

    vv.push_back(make_pair(0, -9));
    push_heap(vv.begin(), vv.end(), ll, place);
    CHECK_PLACE(vv);

    vv.push_back(make_pair(-1, -9));
    push_heap(vv.begin(), vv.end(), ll, place);
    CHECK(vv[0].first == -1);
    CHECK(vv[1].first == 0);
    CHECK_PLACE(vv);

    vv.push_back(make_pair(1, -9));
    push_heap(vv.begin(), vv.end(), ll, place);
    CHECK(vv[0].first == -1);
    CHECK(vv[1].first == 0);
    CHECK(vv[2].first == 1);
    CHECK_PLACE(vv);

    pop_heap(vv.begin(), vv.end(), ll, place);
    CHECK(vv[0].first == 0);
    CHECK(vv[1].first == 1);
    CHECK(vv[2].first == -1);
    vv.pop_back();
    CHECK_PLACE(vv);

    vv.push_back(make_pair(10, -9));
    push_heap(vv.begin(), vv.end(), ll, place);
    CHECK_PLACE(vv);
    vv.push_back(make_pair(8, -9));
    push_heap(vv.begin(), vv.end(), ll, place);
    CHECK(vv[0].first == 0);
    CHECK(vv[1].first == 1);
    CHECK(vv[2].first == 10);
    CHECK(vv[3].first == 8);
    CHECK_PLACE(vv);

    vv[2].first = -2;
    change_heap(vv.begin(), vv.end(), vv.begin() + 2, ll, place);
    CHECK(vv[0].first == -2);
    CHECK(vv[1].first == 1);
    CHECK(vv[2].first == 0);
    CHECK(vv[3].first == 8);
    CHECK_PLACE(vv);

    pop_heap(vv.begin(), vv.end(), ll, place);
    CHECK(vv[0].first == 0);
    CHECK(vv[1].first == 1);
    CHECK(vv[2].first == 8);
    CHECK(vv[3].first == -2);
    vv.pop_back();
    CHECK_PLACE(vv);

    remove_heap(vv.begin(), vv.end(), vv.begin() + 1, ll, place);
    CHECK(vv[0].first == 0);
    CHECK(vv[1].first == 8);
    CHECK(vv[2].first == 1);
    vv.pop_back();
    CHECK_PLACE(vv);


    Vector<int> pq;

    pq.push_back(0);
    push_heap(pq.begin(), pq.end(), l);

    pq.push_back(-1);
    push_heap(pq.begin(), pq.end(), l);
    CHECK(pq[0] == -1);
    CHECK(pq[1] == 0);

    pq.push_back(1);
    push_heap(pq.begin(), pq.end(), l);
    CHECK(pq[0] == -1);
    CHECK(pq[1] == 0);
    CHECK(pq[2] == 1);

    pop_heap(pq.begin(), pq.end(), l);
    pq.pop_back();
    CHECK(pq[0] == 0);
    CHECK(pq[1] == 1);
    CHECK(pq.size() == 2);

    pq.push_back(10);
    push_heap(pq.begin(), pq.end(), l);
    pq.push_back(8);
    push_heap(pq.begin(), pq.end(), l);
    CHECK(pq[0] == 0);
    CHECK(pq[1] == 1);
    CHECK(pq[2] == 10);
    CHECK(pq[3] == 8);
    CHECK(pq.size() == 4);

    pq[2] = -2;
    change_heap(pq.begin(), pq.end(), pq.begin() + 2, l);
    CHECK(pq[0] == -2);
    CHECK(pq[1] == 1);
    CHECK(pq[2] == 0);
    CHECK(pq[3] == 8);

    pop_heap(pq.begin(), pq.end(), l);
    pq.pop_back();
    CHECK(pq[0] == 0);
    CHECK(pq[1] == 1);
    CHECK(pq[2] == 8);
    CHECK(pq.size() == 3);


    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(HeapTest)
CLICK_ENDDECLS
