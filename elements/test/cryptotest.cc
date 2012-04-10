// -*- c-basic-offset: 4 -*-
/*
 * cryptotest.{cc,hh} -- regression test element for crypto functions
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
#include "cryptotest.hh"
#include <click/md5.h>
#include <click/error.hh>
CLICK_DECLS

CryptoTest::CryptoTest()
{
}

static int
md5_test(const char *s, size_t len, const char *expected_digest,
	 ErrorHandler *errh, const char *file, int line)
{
    md5_state_t md5s;
    if (md5_init(&md5s) < 0) {
	errh->warning("%s:%d: MD5 initialization failed", file, line);
	return 0;
    }

    // In the Linux kernel we can't MD5 a constant string (virtual memory
    // stuff).
    char *x = new char[len];
    memcpy(x, s, len);
    md5_append(&md5s, reinterpret_cast<const unsigned char *>(x), len);
    delete[] x;

    unsigned char digest[16];
    md5_finish(&md5s, digest);
    md5_free(&md5s);

    if (memcmp(digest, expected_digest, 16) != 0)
	return errh->error("%s:%d: bad MD5 digest for %<%#.16s...%>, got %s", file, line, s, String(digest, 16).quoted_hex().lower().substring(2, -1).c_str());
    return 0;
}

int
CryptoTest::initialize(ErrorHandler *errh)
{
    if (md5_test("Marriage\n\n\nThis institution,\nperhaps one should say enterprise\nout of respect for which\none says one need not change one's mind\n", 128, "\x93\x48\x6a\xc6\xe0\x32\x44\xf0\x32\xb3\x24\xba\x4a\xde\x28\x43", errh, __FILE__, __LINE__) < 0)
	return -1;

    if (md5_test("This is a test\n", 15, "\xff\x22\x94\x13\x36\x95\x60\x98\xae\x9a\x56\x42\x89\xd1\xbf\x1b", errh, __FILE__, __LINE__) < 0)
	return -1;

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(CryptoTest)
CLICK_ENDDECLS
