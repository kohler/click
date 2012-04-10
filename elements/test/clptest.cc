// -*- c-basic-offset: 4 -*-
/*
 * clptest.{cc,hh} -- regression test element for CLP
 * Eddie Kohler
 *
 * Copyright (c) 2008 Regents of the University of California
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
#include "clptest.hh"
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/clp.h>
CLICK_DECLS

#define Clp_ValAnimal		Clp_ValFirstUser

CLPTest::CLPTest()
{
}

static const Clp_Option options_x1[] = {
    { "a", 0, 1, 0, 0 },
    { 0, 'a', 2, 0, 0 }
};

static const char * const args_x1[] = {
    "--a",
    "-a"
};


static const Clp_Option options_x2[] = {
    { 0, 'a', 1, 0, 0 },
    { 0, 0x03B1, 2, 0, 0 },	// GREEK SMALL LETTER ALPHA
    { 0, 0x0430, 3, 0, 0 }	// CYRILLIC SMALL LETTER A
};

static const char * const args_x2[] = {
    "-\316\261a\320\260b",
    "-\316",
    "-\316\261",
    "-a"
};


static const Clp_Option options_x3[] = {
    { "a", 0, 1, 0, 0 },
    { "abc", 0, 2, 0, 0 },
    { "abd", 0, 3, 0, 0 },
    { "abde", 0, 4, 0, 0 }
};

static const char * const args_x3[] = {
    "--a",
    "--ab",
    "--abc",
    "--abd",
    "--abde"
};


static const Clp_Option options_x4[] = {
    { "ab", 0, 1, 0, 0 },
    { 0, 'a', 2, 0, 0 },
};

static const char * const args_x4[] = {
    "--a",
    "-a",
    "--ab",
    "-ab"
};


static const Clp_Option options_x5[] = {
    { "a", 'a', 1, Clp_ValInt, 0 },
    { "b", 'b', 2, Clp_ValInt, Clp_Optional },
    { "no-c", 'c', 3, 0, 0 }
};

static const char * const args_x5[] = {
    "-a1",
    "-a",
    "2",
    "--a3",
    "--a=4",
    "--a",
    "5",
    "-ax",
    "-b1",
    "-b",
    "2",
    "--b=3",
    "--b",
    "4",
    "--c",
    "--no-c",
    "-c"
};


static const Clp_Option options_x6[] = {
    { "art", 'a', 1, Clp_ValInt, Clp_Negate },
    { "b", 'b', 2, Clp_ValInt, 0 }
};

static const char * const args_x6[] = {
    "-a1",
    "+art",
    "--no-a=1",
    "--no-art",
    "--no-a",
    "-b2",
    "+b",
    "--no-b"
};


static const Clp_Option options_x7[] = {
    { "art", 'a', 1, Clp_ValStringNotOption, Clp_PreferredMatch },
    { "ar", 0, 2, 0, 0 },
    { "artifex", 0, 3, 0, 0 }
};

static const char * const args_x7[] = {
    "-a--ar",
    "-aar",
    "-a",
    "--ar",
    "--art=--ar",
    "--art=ar",
    "--art",
    "--artif"
};


static const Clp_Option options_x8[] = {
    { "animal", 'a', 1, Clp_ValAnimal, 0 }
};

static const char * const args_x8[] = {
    "--animal=cat",
    "--animal=cattle",
    "--animal=dog",
    "--animal=d",
    "--animal=c",
    "--animal=4"
};


extern "C" {
void clptest_errh(Clp_Parser *clp, const char *error) {
    StringAccum *sa = (StringAccum *) clp->user_data;
    *sa << error;
}
}

#define CHECK(x) if (!(x)) errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);
#define CHECK_DATA(x, y, l) CHECK(memcmp((x), (y), (l)) == 0)
#define nelem(x) (sizeof(x) / sizeof((x)[0]))

int
CLPTest::initialize(ErrorHandler *errh)
{
    Clp_Parser *clp = Clp_NewParser(0, 0, 0, 0);
    StringAccum sa;
    clp->user_data = &sa;
    Clp_SetUTF8(clp, 1);
    Clp_SetErrorHandler(clp, clptest_errh);

    // Test 1
    Clp_SetOptions(clp, nelem(options_x1), options_x1);
    Clp_SetArguments(clp, nelem(args_x1), args_x1);
    CHECK(sa.take_string() == "");
    CHECK(Clp_Next(clp) == 1);
    CHECK(Clp_Next(clp) == 2);
    CHECK(Clp_Next(clp) == Clp_Done);

    // Test 1b
    Clp_SetOptionChar(clp, '-', Clp_Short | Clp_Long);
    CHECK(sa.take_string().find_left("1-char long name conflicts") >= 0);

    // Test 2
    Clp_SetOptionChar(clp, '-', Clp_Short);
    Clp_SetOptions(clp, nelem(options_x2), options_x2);
    Clp_SetArguments(clp, nelem(args_x2), args_x2);
    CHECK(sa.take_string() == "");
    CHECK(Clp_Next(clp) == 2);
    CHECK(Clp_Next(clp) == 1);
    CHECK(Clp_Next(clp) == 3);
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "unrecognized option \342\200\230-b\342\200\231\n");
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "unrecognized option \342\200\230-\357\277\275\342\200\231\n");
    CHECK(Clp_Next(clp) == 2);
    CHECK(Clp_Next(clp) == 1);
    CHECK(Clp_Next(clp) == Clp_Done);

    // Test 3
    Clp_SetOptions(clp, nelem(options_x3), options_x3);
    Clp_SetArguments(clp, nelem(args_x3), args_x3);
    CHECK(sa.take_string() == "");
    CHECK(Clp_Next(clp) == 1);
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "option \342\200\230--ab\342\200\231 is ambiguous\n(Possibilities are \342\200\230--abc\342\200\231, \342\200\230--abd\342\200\231, and \342\200\230--abde\342\200\231.)\n");
    CHECK(Clp_Next(clp) == 2);
    CHECK(Clp_Next(clp) == 3);
    CHECK(Clp_Next(clp) == 4);
    CHECK(Clp_Next(clp) == Clp_Done);

    // Test 4
    Clp_SetOptions(clp, nelem(options_x4), options_x4);
    Clp_SetArguments(clp, nelem(args_x4), args_x4);
    CHECK(sa.take_string() == "");
    CHECK(Clp_Next(clp) == 1);
    CHECK(Clp_Next(clp) == 2);
    CHECK(Clp_Next(clp) == 1);
    CHECK(Clp_Next(clp) == 2);
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "unrecognized option \342\200\230-b\342\200\231\n");
    CHECK(Clp_Next(clp) == Clp_Done);

    // Test 4b
    Clp_SetOptionChar(clp, '-', Clp_Long | Clp_Short);
    Clp_SetArguments(clp, nelem(args_x4), args_x4);
    CHECK(sa.take_string() == "");
    CHECK(Clp_Next(clp) == 1);
    CHECK(Clp_Next(clp) == 2);
    CHECK(Clp_Next(clp) == 1);
    CHECK(Clp_Next(clp) == 1);
    CHECK(Clp_Next(clp) == Clp_Done);

    // Test 5
    Clp_SetOptionChar(clp, '-', Clp_Short);
    Clp_SetOptions(clp, nelem(options_x5), options_x5);
    Clp_SetArguments(clp, nelem(args_x5), args_x5);
    CHECK(sa.take_string() == "");
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->val.i == 1);
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->val.i == 2);
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "unrecognized option \342\200\230--a3\342\200\231\n");
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->val.i == 4);
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->val.i == 5);
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "\342\200\230-a\342\200\231 expects an integer, not \342\200\230x\342\200\231\n");
    CHECK(Clp_Next(clp) == 2 && clp->have_val);
    CHECK(1 && clp->have_val && clp->val.i == 1);
    CHECK(Clp_Next(clp) == 2 && !clp->have_val);
    CHECK(Clp_Next(clp) == Clp_NotOption && String(clp->vstr) == "2");
    CHECK(Clp_Next(clp) == 2 && clp->have_val && clp->val.i == 3);
    CHECK(Clp_Next(clp) == 2 && !clp->have_val);
    CHECK(Clp_Next(clp) == Clp_NotOption && String(clp->vstr) == "4");
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "unrecognized option \342\200\230--c\342\200\231\n");
    CHECK(Clp_Next(clp) == 3 && clp->negated);
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "unrecognized option \342\200\230-c\342\200\231\n");
    CHECK(Clp_Next(clp) == Clp_Done);

    // Test 6
    Clp_SetOptionChar(clp, '+', Clp_LongNegated);
    Clp_SetOptions(clp, nelem(options_x6), options_x6);
    Clp_SetArguments(clp, nelem(args_x6), args_x6);
    CHECK(sa.take_string() == "");
    CHECK(Clp_Next(clp) == 1 && !clp->negated && clp->have_val && clp->val.i == 1);
    CHECK(Clp_Next(clp) == 1 && clp->negated && !clp->have_val);
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "\342\200\230--no-art\342\200\231 can\342\200\231t take an argument\n");
    CHECK(Clp_Next(clp) == 1 && clp->negated && !clp->have_val);
    CHECK(Clp_Next(clp) == 1 && clp->negated && !clp->have_val);
    CHECK(Clp_Next(clp) == 2 && !clp->negated && clp->have_val && clp->val.i == 2);
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "unrecognized option \342\200\230+b\342\200\231\n");
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "unrecognized option \342\200\230--no-b\342\200\231\n");
    CHECK(Clp_Next(clp) == Clp_Done);

    // Test 7
    Clp_SetOptions(clp, nelem(options_x7), options_x7);
    Clp_SetArguments(clp, nelem(args_x7), args_x7);
    CHECK(sa.take_string() == "");
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->vstr == String("--ar"));
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->vstr == String("ar"));
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "\342\200\230-a\342\200\231 requires a non-option argument\n");
    CHECK(Clp_Next(clp) == 2);
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->vstr == String("--ar"));
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->vstr == String("ar"));
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "\342\200\230--art\342\200\231 requires a non-option argument\n");
    CHECK(Clp_Next(clp) == 3);
    CHECK(Clp_Next(clp) == Clp_Done);

    // Test 8
    Clp_AddStringListType(clp, Clp_ValAnimal, Clp_AllowNumbers,
			  "cat", 1, "cattle", 2, "dog", 3, (const char *) 0);
    Clp_SetOptions(clp, nelem(options_x8), options_x8);
    Clp_SetArguments(clp, nelem(args_x8), args_x8);
    CHECK(sa.take_string() == "");
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->val.i == 1);
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->val.i == 2);
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->val.i == 3);
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->val.i == 3);
    CHECK(Clp_Next(clp) == Clp_BadOption);
    CHECK(sa.take_string() == "option \342\200\230--animal\342\200\231 value \342\200\230c\342\200\231 is ambiguous\n(Possibilities are \342\200\230cat\342\200\231 and \342\200\230cattle\342\200\231.)\n");
    CHECK(Clp_Next(clp) == 1 && clp->have_val && clp->val.i == 4);
    CHECK(Clp_Next(clp) == Clp_Done);

    Clp_DeleteParser(clp);
    if (!errh->nerrors()) {
	errh->message("All tests pass!");
	return 0;
    } else
	return -1;
}

EXPORT_ELEMENT(CLPTest)
ELEMENT_REQUIRES(userlevel)
CLICK_ENDDECLS
