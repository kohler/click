/*
 * listtest.{cc,hh} -- regression test element for ListTest<T>
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
#include "listtest.hh"
#include <click/list.hh>
#include <click/error.hh>
#if CLICK_USERLEVEL
# include <sys/time.h>
# include <sys/resource.h>
# include <unistd.h>
#endif
CLICK_DECLS

ListTest::ListTest()
{
}

namespace {
struct stringlistentry {
    String s;
    List_member<stringlistentry> link;
    stringlistentry() {
    }
    stringlistentry(const String &str)
	: s(str) {
    }
};

typedef List<stringlistentry, &stringlistentry::link> stringlist;
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x)
#define CHECKI(x, i) if (!(x)) return errh->error("%s:%d: test `%s' (%d) failed", __FILE__, __LINE__, #x, i)

int
ListTest::initialize(ErrorHandler *errh)
{
    stringlistentry x[12];
    x[0].s = "A";
    x[1].s = "B";
    x[2].s = "Anne Elizabeth Dudfield";
    x[3].s = "facker";
    x[4].s = "McArdle";
    x[5].s = "Zoom";
    x[6].s = "==++";

    stringlist l;
    for (int i = 0; i < 7; ++i)
	l.push_back(&x[i]);

    int i = 0;
    for (stringlist::iterator it = l.begin(); it != l.end(); ++it, ++i)
	CHECKI(it->s == x[i].s, i);
    CHECK(l.size() == 7);
    CHECK(i == 7);

    l.pop_back();
    i = 0;
    for (stringlist::iterator it = l.begin(); it != l.end(); ++it, ++i)
	CHECKI(it->s == x[i].s, i);
    CHECK(l.size() == 6);
    CHECK(i == 6);

    l.pop_front();
    i = 0;
    for (stringlist::iterator it = l.begin(); it != l.end(); ++it, ++i)
	CHECKI(it->s == x[i + 1].s, i);
    CHECK(l.size() == 5);
    CHECK(i == 5);

    l.erase(l.begin() + 2);
    i = 0;
    for (stringlist::iterator it = l.begin(); it != l.end(); ++it, ++i)
	CHECKI(it->s == x[i < 2 ? i + 1 : i + 2].s, i);
    CHECK(l.size() == 4);
    CHECK(i == 4);

    l.clear();
    CHECK(l.size() == 0);

    for (i = 3; i >= 0; --i)
	l.push_front(&x[i]);
    CHECK(l.size() == 4);

    i = 0;
    for (stringlist::iterator it = l.end() - 1; it != l.begin(); ++i) {
	--it;
	CHECKI(it->s == x[2 - i].s, i);
    }
    CHECK(i == 3);

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(ListTest)
CLICK_ENDDECLS
