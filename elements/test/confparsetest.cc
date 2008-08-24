// -*- c-basic-offset: 4 -*-
/*
 * confparsetest.{cc,hh} -- regression test element for configuration parsing
 * Eddie Kohler
 *
 * Copyright (c) 2007 Regents of the University of California
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
#include "confparsetest.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
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
    u32 = 97;
    CHECK(cp_integer("0", &i32) == true && i32 == 0);
    CHECK(cp_integer("-0", &i32) == true && i32 == 0);
    CHECK(u32 == 97);
    CHECK(cp_integer("0", &u32) == true && u32 == 0);
    CHECK(cp_integer("-0", &u32) == false);
    CHECK(cp_integer("4294967294", &u32) == true && u32 == 4294967294U);
    CHECK(cp_integer("0xFFFFFFFE", &u32) == true && u32 == 4294967294U);
    CHECK(cp_integer("4294967296", &u32) == true && u32 == 4294967295U && cp_errno == CPE_OVERFLOW);
    CHECK(cp_integer("42949672961939", &u32) == true && u32 == 4294967295U && cp_errno == CPE_OVERFLOW);
    CHECK(cp_integer("0xFFFFFFFFF", &u32) == true && u32 == 4294967295U && cp_errno == CPE_OVERFLOW);
    CHECK(cp_integer("4294967296", &i32) == true && i32 == 2147483647 && cp_errno == CPE_OVERFLOW);
    CHECK(cp_integer("2147483647", &i32) == true && i32 == 2147483647 && cp_errno == CPE_OK);
    CHECK(cp_integer("-2147483648", &i32) == true && i32 == -2147483647 - 1 && cp_errno == CPE_OK);
    CHECK(cp_integer("-4294967296", &i32) == true && i32 == -2147483647 - 1 && cp_errno == CPE_OVERFLOW);
    const char *s = "-127 ";
    CHECK(cp_integer(s, s + strlen(s) - 1, 10, &i32) == s + 4 && i32 == -127);
    CHECK(cp_integer(String(s), &i32) == false && cp_errno == CPE_FORMAT);

#if HAVE_LONG_LONG && SIZEOF_LONG_LONG == 8
    long long ll;
    unsigned long long ull;
    CHECK(cp_integer("9223372036854775807", &ll) == true && ll == 0x7FFFFFFFFFFFFFFFULL);
    CHECK(cp_integer("-9223372036854775808", &ll) == true && ll == (long long) 0x8000000000000000ULL);
    CHECK(cp_integer("18446744073709551616", &ull) == true && ull == 0xFFFFFFFFFFFFFFFFULL && cp_errno == CPE_OVERFLOW);
#endif
    
    CHECK(cp_real2("-0.5", 1, &i32) == true && i32 == -1);
    CHECK(cp_seconds_as("3600", 0, &u32) == true && u32 == 3600);
    CHECK(cp_seconds_as("3600s", 0, &u32) == true && u32 == 3600);
    CHECK(cp_seconds_as("3.6e6 msec", 0, &u32) == true && u32 == 3600);
    CHECK(cp_seconds_as("60m", 0, &u32) == true && u32 == 3600);
    CHECK(cp_seconds_as("1 hr", 0, &u32) == true && u32 == 3600);

#if HAVE_FLOAT_TYPES
    double d;
    CHECK(cp_seconds("3600", &d) == true && d == 3600);
    CHECK(cp_seconds("3600s", &d) == true && d == 3600);
    CHECK(cp_seconds("3.6e6 msec", &d) == true && d == 3600);
    CHECK(cp_seconds("60m", &d) == true && d == 3600);
    CHECK(cp_seconds("1 hr", &d) == true && d == 3600);
#endif

#if CLICK_IP6
    {
	IP6Address a;
	CHECK(cp_ip6_address("1080:0:0:0:8:800:200C:417a", &a, this) == true
	      && a.data32()[0] == ntohl(0x10800000)
	      && a.data32()[1] == ntohl(0x00000000)
	      && a.data32()[2] == ntohl(0x00080800)
	      && a.data32()[3] == ntohl(0x200C417a));
	CHECK(cp_ip6_address("1080::8:800:200C:417a", &a, this) == true
	      && a.data32()[0] == ntohl(0x10800000)
	      && a.data32()[1] == ntohl(0x00000000)
	      && a.data32()[2] == ntohl(0x00080800)
	      && a.data32()[3] == ntohl(0x200C417a));
	CHECK(cp_ip6_address("::13.1.68.3", &a, this) == true
	      && a.data32()[0] == 0x00000000
	      && a.data32()[1] == 0x00000000
	      && a.data32()[2] == 0x00000000
	      && a.data32()[3] == ntohl(0x0D014403));
	CHECK(cp_ip6_address("::ffff:129.144.52.38", &a, this) == true
	      && a.data32()[0] == 0x00000000
	      && a.data32()[1] == 0x00000000
	      && a.data32()[2] == ntohl(0x0000FFFF)
	      && a.data32()[3] == ntohl(0x81903426));
	IPAddress a4;
	if (cp_ip_address("ip4_addr", &a4, this) == true)
	    CHECK(cp_ip6_address("0::ip4_addr", &a, this) == true
		  && a.data32()[0] == 0x00000000 && a.data32()[1] == 0x00000000
		  && a.data32()[2] == 0x00000000
		  && a.data32()[3] == a4.addr());
    }
#endif

    Timestamp t = Timestamp(0, 0) - Timestamp::make_msec(1001);
    CHECK(t.sec() == -2 && t.usec() == 999000);
    CHECK(t.unparse() == "-1.001000");
    Timestamp t2 = Timestamp(-10, 0);
    CHECK(t2.sec() == -10 && t2.subsec() == 0);
    CHECK(t2.unparse() == "-10.000000");
    CHECK(t2 < t);
    CHECK((-t2).unparse() == "10.000000");
    CHECK((-t).unparse() == "1.001000");
    CHECK(-t2 > t);
    CHECK((-t + t2).unparse() == "-8.999000");
    t = Timestamp::make_msec(999);
    CHECK((t + t).unparse() == "1.998000");
    CHECK((-t - t).unparse() == "-1.998000");
    CHECK(-t == Timestamp::make_msec(-999));
    CHECK(t == Timestamp::make_usec(999000));
    CHECK(-t == Timestamp::make_usec(-999000));
    CHECK(t == Timestamp::make_nsec(999000000));
    CHECK(-t == Timestamp::make_nsec(-999000000));
    CHECK(t.msecval() == 999);
    CHECK(t.usecval() == 999000);
    CHECK(t.nsecval() == 999000000);
    CHECK((-t).msecval() == -999);
    CHECK((-t).usecval() == -999000);
    CHECK((-t).nsecval() == -999000000);

    // some string tests for good measure
    CHECK(String("abcdef").substring(-3) == "def");
    CHECK(String("abc").substring(-3) == "abc");
    CHECK(String("ab").substring(-3) == "ab");
    CHECK(String("abcdef").substring(-3, -2) == "d");
    CHECK(String("abcdef").substring(-3, 2) == "de");
    CHECK(String("abcdef").substring(2, -3) == "c");
    CHECK(String("abcdef").substring(2, 3) == "cde");
    CHECK(String("abcdef").substring(2, 10) == "cdef");
    CHECK(String("abcdef").substring(-2, 10) == "ef");
    CHECK(String("abcdef").substring(-10, -3) == "abc");
    CHECK(String("abcdef").substring(-10, -9) == "");
    CHECK(String("abcdef").substring(10, -9) == "");

    String x("abcdefghijklmn");
    x.append(x.data() + 1, 12);
    CHECK(x == "abcdefghijklmnbcdefghijklm");
    x += x;
    CHECK(x == "abcdefghijklmnbcdefghijklmabcdefghijklmnbcdefghijklm");

    StringAccum xx(24);
    xx << "abcdefghijklmn";
    CHECK(xx.capacity() - xx.length() < 12);
    xx.append(xx.data() + 1, 12);
    CHECK(strcmp(xx.c_str(), "abcdefghijklmnbcdefghijklm") == 0);
    xx << xx;
    CHECK(strcmp(xx.c_str(), "abcdefghijklmnbcdefghijklmabcdefghijklmnbcdefghijklm") == 0);

    // String hash codes
    {
	static const char hash1[] = "boot_countXXXXXXYboot_countYYYYYZZboot_countZZZZWWWboot_countWWW";
	String s1 = String::stable_string(hash1, 10);
	String s2 = String::stable_string(hash1 + 17, 10);
	String s3 = String::stable_string(hash1 + 34, 10);
	String s4 = String::stable_string(hash1 + 51, 10);
	CHECK(s1 == s2 && s2 == s3 && s3 == s4 && s4 == s1);
	CHECK(s1.hashcode() == s2.hashcode());
	CHECK(s1.hashcode() == s3.hashcode());
	CHECK(s1.hashcode() == s4.hashcode());
	CHECK(((uintptr_t) s1.data() & 3) != ((uintptr_t) s2.data() & 3));
	CHECK(((uintptr_t) s1.data() & 3) != ((uintptr_t) s3.data() & 3));
	CHECK(((uintptr_t) s2.data() & 3) != ((uintptr_t) s3.data() & 3));
	CHECK(((uintptr_t) s1.data() & 3) != ((uintptr_t) s4.data() & 3));
	CHECK(((uintptr_t) s2.data() & 3) != ((uintptr_t) s4.data() & 3));
	CHECK(((uintptr_t) s3.data() & 3) != ((uintptr_t) s4.data() & 3));
    }
    
    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(ConfParseTest)
CLICK_ENDDECLS
