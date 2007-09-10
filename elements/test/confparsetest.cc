// -*- c-basic-offset: 4 -*-
/*
 * confparsetest.{cc,hh} -- regression test element for configuration parsing
 * Eddie Kohler
 *
 * Copyright (c) 2007 Regents of the University of California
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
#include "confparsetest.hh"
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

ConfParseTest::ConfParseTest()
{
}

ConfParseTest::~ConfParseTest()
{
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);

int
ConfParseTest::initialize(ErrorHandler *errh)
{
    CHECK(cp_uncomment("  a  b  ") == "a  b");
    CHECK(cp_uncomment("  a /* whatever */   // whatever\n    b  ") == "a b");
    CHECK(cp_uncomment("  \" /*???  */ \"  ") == "\" /*???  */ \"");
    CHECK(cp_unquote("\"\\n\" abc /* 123 */ '/* def */'") == "\n abc /* def */");

    Vector<String> v;
    cp_argvec("a, b, c", v);
    CHECK(v.size() == 3);
    CHECK(v[0] == "a");
    CHECK(v[1] == "b");
    CHECK(v[2] == "c");
    cp_argvec("  a /*?*/ b,  c, ", v);
    CHECK(v.size() == 5);
    CHECK(v[3] == "a b");
    CHECK(v[4] == "c");
    cp_argvec("\"x, y\" // ?", v);
    CHECK(v.size() == 6);
    CHECK(v[5] == "\"x, y\"");
    cp_spacevec("a  b, c", v);
    CHECK(v.size() == 9);
    CHECK(v[6] == "a");
    CHECK(v[7] == "b,");
    CHECK(v[8] == "c");
    cp_spacevec("  'a /*?*/ b'c", v);
    CHECK(v.size() == 10);
    CHECK(v[9] == "'a /*?*/ b'c");

    int32_t i32;
    uint32_t u32;
    CHECK(cp_real2("-0.5", 1, &i32) == true && i32 == -1);
    CHECK(cp_seconds_as("3600", 0, &u32) == true && u32 == 3600);
    CHECK(cp_seconds_as("3600s", 0, &u32) == true && u32 == 3600);
    CHECK(cp_seconds_as("3.6e6 msec", 0, &u32) == true && u32 == 3600);
    CHECK(cp_seconds_as("60m", 0, &u32) == true && u32 == 3600);
    CHECK(cp_seconds_as("1 hr", 0, &u32) == true && u32 == 3600);

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(ConfParseTest)
CLICK_ENDDECLS
