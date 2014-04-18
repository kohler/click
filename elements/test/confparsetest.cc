// -*- c-basic-offset: 4 -*-
/*
 * confparsetest.{cc,hh} -- regression test element for configuration parsing
 * Eddie Kohler
 *
 * Copyright (c) 2007-2011 Regents of the University of California
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#if HAVE_IP6
# include <click/ip6address.hh>
#endif
#if CLICK_USERLEVEL
# include <click/userutils.hh>
#endif
CLICK_DECLS

namespace {
class RecordErrorHandler : public ErrorHandler { public:
    RecordErrorHandler() {
    }
    void *emit(const String &str, void *, bool) {
	_sa.append(skip_anno(str.begin(), str.end()), str.end());
	return 0;
    }
    String take_string() {
	return _sa.take_string();
    }
    StringAccum _sa;
};
}

ConfParseTest::ConfParseTest()
{
}

#define CHECK(x) do {				\
	if (!(x))				\
	    return errh->error("%s:%d: test %<%s%> failed", __FILE__, __LINE__, #x); \
    } while (0)
#define CHECK_ERR(x, errstr) do {		\
	if (!(x))				\
	    return errh->error("%s:%d: test %<%s%> failed", __FILE__, __LINE__, #x); \
	String msg(rerrh.take_string());		\
	if (!msg.equals(errstr, -1))	\
	    return errh->error("%s:%d: test %<%s%> produces unexpected error message %<%s%>", __FILE__, __LINE__, #x, msg.c_str()); \
    } while (0)

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

    // cp_integer parsing
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
    CHECK(cp_integer("2147483648", &i32) == true && i32 == 2147483647 && cp_errno == CPE_OVERFLOW);
    CHECK(cp_integer("-2147483648", &i32) == true && i32 == -2147483647 - 1 && cp_errno == CPE_OK);
    CHECK(cp_integer("-4294967296", &i32) == true && i32 == -2147483647 - 1 && cp_errno == CPE_OVERFLOW);
    const char *s = "-127 ";
    CHECK(cp_integer(s, s + strlen(s) - 1, 10, &i32) == s + 4 && i32 == -127);
    CHECK(cp_integer(String(s), &i32) == false && cp_errno == CPE_FORMAT);
#if HAVE_LONG_LONG && SIZEOF_LONG_LONG == 8
    {
	long long ll;
	unsigned long long ull;
	CHECK(cp_integer("9223372036854775807", &ll) == true && ll == 0x7FFFFFFFFFFFFFFFULL);
	CHECK(cp_integer("-9223372036854775808", &ll) == true && ll == (long long) 0x8000000000000000ULL);
	CHECK(cp_integer("18446744073709551616", &ull) == true && ull == 0xFFFFFFFFFFFFFFFFULL && cp_errno == CPE_OVERFLOW);
    }
#endif

    // IntArg parsing
    RecordErrorHandler rerrh;
    Args args(0, &rerrh);
    u32 = 97;
    IntArg ia;
    CHECK(IntArg().parse("0", i32) == true && i32 == 0);
    CHECK(IntArg().parse("-0", i32) == true && i32 == 0);
    CHECK(IntArg().parse("-5", i32) == true && i32 == -5);
    CHECK(u32 == 97);
    CHECK_ERR(IntArg().parse("aoeu", u32, args) == false && u32 == 97, "invalid number");
    CHECK(IntArg().parse("0", u32) == true && u32 == 0);
    CHECK(IntArg().parse("-0", u32) == false);
    CHECK(IntArg().parse("4294967294", u32) == true && u32 == 4294967294U);
    CHECK(IntArg().parse("0xFFFFFFFE", u32) == true && u32 == 4294967294U);
    CHECK_ERR(ia.parse("4294967296", u32, args) == false && u32 == 4294967294U, "out of range, bound 4294967295");
    CHECK(IntArg().parse_saturating("4294967296", u32) == true && u32 == 4294967295U);
    u32 = 97;
    CHECK_ERR(ia.parse("42949672961939", u32, args) == false && u32 == 97, "out of range, bound 4294967295");
    CHECK_ERR(ia.parse_saturating("42949672961939", u32, args) == true && u32 == 4294967295U, "");
    CHECK(IntArg().parse("0xFFFFFFFFF", u32) == false);
    CHECK(IntArg().parse_saturating("0xFFFFFFFFF", u32) == true && u32 == 0xFFFFFFFFU);
    CHECK_ERR(ia.parse("4294967296", i32, args) == false, "out of range, bound 2147483647");
    CHECK_ERR(ia.parse_saturating("4294967296", i32, args) == true && i32 == 2147483647, "");
    CHECK_ERR(ia.parse("2147483647", i32, args) == true && i32 == 2147483647, "");
    CHECK_ERR(ia.parse("2147483648", i32, args) == false && i32 == 2147483647, "out of range, bound 2147483647");
    CHECK_ERR(ia.parse_saturating("2147483648", i32, args) == true && i32 == 2147483647, "");
    CHECK_ERR(ia.parse("-2147483648", i32, args) == true && i32 == -2147483647 - 1, "");
    CHECK_ERR(ia.parse("-2147483649", i32, args) == false && i32 == -2147483647 - 1, "out of range, bound -2147483648");
    i32 = 0;
    CHECK_ERR(ia.parse_saturating("-2147483649", i32, args) == true && i32 == -2147483647 - 1, "");
    CHECK_ERR(ia.parse("-4294967296", i32, args) == false && i32 == -2147483647 - 1, "out of range, bound -2147483648");
    int8_t i8 = 0;
    CHECK_ERR(ia.parse("97", i8, args) == true && i8 == 97, "");
    CHECK_ERR(ia.parse("128", i8, args) == false && i8 == 97, "out of range, bound 127");
    CHECK_ERR(ia.parse_saturating("128", i8, args) == true && i8 == 127, "");
#if HAVE_INT64_TYPES
    int64_t i64;
    uint64_t u64 = 0;
    CHECK_ERR(ia.parse("9223372036854775807", i64, args) == true && i64 == int64_t(0x7FFFFFFFFFFFFFFFULL), "");
    CHECK_ERR(ia.parse("-9223372036854775808", i64, args) == true && i64 == int64_t(0x8000000000000000ULL), "");
    CHECK_ERR(ia.parse("-5", i64, args) == true && i64 == -5, "");
    CHECK_ERR(ia.parse("18446744073709551616", u64, args) == false && u64 == 0, "out of range, bound 18446744073709551615");
    CHECK_ERR(ia.parse_saturating("18446744073709551616", u64, args) == true && u64 == 0xFFFFFFFFFFFFFFFFULL, "");
#endif

    CHECK_ERR(BoundedIntArg(0, 10).parse("10", i32, args) == true && i32 == 10, "");
    CHECK_ERR(BoundedIntArg(0, 9).parse("10", i32, args) == false && i32 == 10, "out of range, bound 9");
    CHECK_ERR(BoundedIntArg(-1, 9).parse("-10", i32, args) == false && i32 == 10, "out of range, bound -1");
    CHECK_ERR(BoundedIntArg(-1000, -90).parse("0xFFFFFF00", u32, args) == false && i32 == 10, "out of range, bound -90");
    CHECK_ERR(BoundedIntArg(0U, 100U).parse("-1", i32, args) == false && i32 == 10, "out of range, bound 0");
    CHECK_ERR(BoundedIntArg(0U, 100U).parse("-1", i32, args) == false && i32 == 10, "out of range, bound 0");
    CHECK_ERR(BoundedIntArg(0U, ~0U).parse("-1", i32, args) == false && i32 == 10, "out of range, bound 0");
    CHECK_ERR(BoundedIntArg(-1, 9).parse("aoeu", i32, args) == false && i32 == 10, "invalid number");

    bool b; (void) b;
    CHECK(FixedPointArg(1).parse("0.5", i32) == true && i32 == 1);
    CHECK(FixedPointArg(1).parse("-0.5", i32) == true && i32 == -1);
#define CHECK_FIXEDPOINT(s, frac_bits, result) { String q = (#s); uint32_t r; \
    if (!FixedPointArg((frac_bits)).parse(q, r)) \
	return errh->error("%s:%d: %<%s%> unparseable", __FILE__, __LINE__, q.c_str()); \
    String qq = cp_unparse_real2(r, (frac_bits)); \
    if (qq != (result)) \
	return errh->error("%s:%d: %<%s%> parsed and unparsed is %<%s%>, should be %<%s%>", __FILE__, __LINE__, q.c_str(), qq.c_str(), result); \
}
    CHECK_FIXEDPOINT(0.418, 8, "0.418");
    CHECK_FIXEDPOINT(0.417, 8, "0.418");
    CHECK_FIXEDPOINT(0.416, 8, "0.414");
    CHECK_FIXEDPOINT(0.42, 8, "0.42");
    CHECK_FIXEDPOINT(0.3, 16, "0.3");
    CHECK_FIXEDPOINT(0.49, 16, "0.49");
    CHECK_FIXEDPOINT(0.499, 16, "0.499");
    CHECK_FIXEDPOINT(0.4999, 16, "0.4999");
    CHECK_FIXEDPOINT(0.49999, 16, "0.49998");
    CHECK_FIXEDPOINT(0.499999, 16, "0.5");
    CHECK_FIXEDPOINT(0.49998, 16, "0.49998");
    CHECK_FIXEDPOINT(0.999999, 16, "1");
#undef CHECK_FIXEDPOINT

    CHECK(cp_real2("-0.5", 1, &i32) == true && i32 == -1);
    CHECK(SecondsArg().parse("3600", u32) == true && u32 == 3600);
    CHECK(cp_seconds_as("3600", 0, &u32) == true && u32 == 3600);
    CHECK(SecondsArg().parse("3600s", u32) == true && u32 == 3600);
    CHECK(SecondsArg().parse("3.6e6 msec", u32) == true && u32 == 3600);
    CHECK(SecondsArg().parse("60m", u32) == true && u32 == 3600);
    CHECK(SecondsArg().parse("1 hr", u32) == true && u32 == 3600);
    CHECK(SecondsArg().parse("1.99 hr", u32) == true && u32 == 7164);
    CHECK(SecondsArg().parse("1.99 s", u32) == true && u32 == 2);
    CHECK(SecondsArg(1).parse("1.99 s", u32) == true && u32 == 20);
    CHECK(SecondsArg(2).parse("1.99 s", u32) == true && u32 == 199);
    CHECK(SecondsArg().parse("1ms", u32) == true && u32 == 0);
    CHECK(SecondsArg(3).parse("1ms", u32) == true && u32 == 1);
    CHECK(SecondsArg(3).parse("1.9ms", u32) == true && u32 == 2);
    CHECK(SecondsArg(6).parse("1ms", u32) == true && u32 == 1000);
    CHECK(SecondsArg(6).parse("1.9ms", u32) == true && u32 == 1900);

#if HAVE_FLOAT_TYPES
    double d;
    CHECK(SecondsArg().parse("3600", d) == true && d == 3600);
    CHECK(SecondsArg().parse("3600s", d) == true && d == 3600);
    CHECK(SecondsArg().parse("3.6e6 msec", d) == true && d == 3600);
    CHECK(SecondsArg().parse("60m", d) == true && d == 3600);
    CHECK(SecondsArg().parse("1 hr", d) == true && d == 3600);
#endif

    BandwidthArg bwarg;
    CHECK(bwarg.parse("8", u32) == true && bwarg.status == NumArg::status_unitless && u32 == 8);
    CHECK(bwarg.parse("8 baud", u32) == true && bwarg.status == NumArg::status_ok && u32 == 1);
    CHECK(bwarg.parse("8Kbps", u32) == true && bwarg.status == NumArg::status_ok && u32 == 1000);
    CHECK(bwarg.parse("8KBps", u32) == true && bwarg.status == NumArg::status_ok && u32 == 8000);

    {
	IPAddress a, m;
	CHECK(IPPrefixArg().parse("18.26.4/24", a, m, this) == true
	      && a.unparse_with_mask(m) == "18.26.4.0/24");
	CHECK(IPPrefixArg().parse("18.26.4/28", a, m, this) == false);
    }

#if HAVE_IP6
    {
	IP6Address a;
	CHECK(cp_ip6_address("1080:0:0:0:8:800:200C:417a", &a, this) == true);
	CHECK(a.data32()[0] == ntohl(0x10800000)
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
	if (IPAddressArg().parse("ip4_addr", a4, this) == true)
	    CHECK(cp_ip6_address("0:::ip4_addr", &a, this) == true
		  && a.data32()[0] == 0x00000000 && a.data32()[1] == 0x00000000
		  && a.data32()[2] == 0x00000000
		  && a.data32()[3] == a4.addr());
	if (IPAddressArg().parse("ip4_addr", a4, this) == true)
	    CHECK(cp_ip6_address("0::ffff:ip4_addr", &a, this) == true
		  && a.data32()[0] == 0x00000000 && a.data32()[1] == 0x00000000
		  && a.data32()[2] == htonl(0x0000FFFFU)
		  && a.data32()[3] == a4.addr());
	IPAddress b4("18.26.4.9");
	CHECK(IP6Address(b4).is_ip4_mapped());
	CHECK(IP6Address(b4).ip4_address() == b4);
	a = IP6Address("::ffff:18.26.4.9");
	CHECK(a.is_ip4_mapped());
	CHECK(a.ip4_address() == b4);
	CHECK(cp_ip6_address("ffff:ffff:ffff:ffff:ffff:ffff::", &a, this) == true
	      && a.data32()[0] == 0xFFFFFFFF
	      && a.data32()[1] == 0xFFFFFFFF
	      && a.data32()[2] == 0xFFFFFFFF
	      && a.data32()[3] == 0);
	CHECK(a.mask_to_prefix_len() == 96);
	CHECK(cp_ip6_address("ffff:ffff:ffff:ffff:ffff:ffff:8000:0", &a, this) == true
	      && a.data32()[0] == 0xFFFFFFFF
	      && a.data32()[1] == 0xFFFFFFFF
	      && a.data32()[2] == 0xFFFFFFFF
	      && a.data32()[3] == htonl(0x80000000));
	CHECK(a.mask_to_prefix_len() == 97);
	CHECK(cp_ip6_address("ffff:ffff:ffff:ffff:ffff:ffff:8000:", &a, this) == false);
	CHECK(cp_ip6_address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", &a, this) == true
	      && a.data32()[0] == 0xFFFFFFFF
	      && a.data32()[1] == 0xFFFFFFFF
	      && a.data32()[2] == 0xFFFFFFFF
	      && a.data32()[3] == 0xFFFFFFFF);
	CHECK(a.mask_to_prefix_len() == 128);
	CHECK(cp_ip6_address("::", &a, this) == true
	      && a.data32()[0] == 0
	      && a.data32()[1] == 0
	      && a.data32()[2] == 0
	      && a.data32()[3] == 0);
	CHECK(a.mask_to_prefix_len() == 0);
	CHECK(cp_ip6_address(":::", &a, this) == false);
	CHECK(cp_ip6_address("8000::1:1:1:1:1:1:1", &a, this) == false);
	CHECK(cp_ip6_address("8000::", &a, this) == true
	      && a.data32()[0] == htonl(0x80000000)
	      && a.data32()[1] == 0
	      && a.data32()[2] == 0
	      && a.data32()[3] == 0);
	CHECK(cp_ip6_address("::8000", &a, this) == true
	      && a.data32()[0] == 0
	      && a.data32()[1] == 0
	      && a.data32()[2] == 0
	      && a.data32()[3] == htonl(0x00008000));
	CHECK(a.mask_to_prefix_len() == -1);
	a = IP6Address::make_inverted_prefix(17);
	CHECK(a.data32()[0] == htonl(0x00007FFF)
	      && a.data32()[1] == 0xFFFFFFFF
	      && a.data32()[2] == 0xFFFFFFFF
	      && a.data32()[3] == 0xFFFFFFFF);
	a = IP6Address::make_inverted_prefix(128);
	CHECK(a.data32()[0] == 0
	      && a.data32()[1] == 0
	      && a.data32()[2] == 0
	      && a.data32()[3] == 0);
    }
#endif

    Timestamp t = Timestamp(0, 0) - Timestamp::make_msec(1001);
    CHECK(t.sec() == -2 && t.usec() == 999000);
    CHECK(t.unparse() == "-1.001000");
#if CLICK_HZ == 1000		/* true at userlevel */
    CHECK(t == Timestamp::make_jiffies((click_jiffies_difference_t) -1001));
    CHECK(t < Timestamp::make_jiffies((click_jiffies_t) -1001));
    CHECK(-t == Timestamp::make_jiffies((click_jiffies_t) 1001));
    CHECK(-t == Timestamp::make_jiffies((click_jiffies_difference_t) 1001));
#endif
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
    t = Timestamp(0, 0) - Timestamp::make_msec(10000001);
    CHECK(t.sec() == -10001 && t.usec() == 999000);
    CHECK(t.subsec() == 999 * Timestamp::subsec_per_msec);

    CHECK(Timestamp(Timestamp::make_nsec(0, 999).timeval()) == Timestamp::make_usec(0, 0));
#if TIMESTAMP_NANOSEC
    CHECK(Timestamp(Timestamp::make_nsec(0, 1).timeval_ceil()) == Timestamp::make_usec(0, 1));
    CHECK(Timestamp(Timestamp::make_nsec(0, 0).timeval_ceil()) == Timestamp::make_usec(0, 0));
#endif

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

    {
	String z;
	z += x;
	CHECK(z == "abcdefghijklmnbcdefghijklmabcdefghijklmnbcdefghijklm");
	CHECK(z.data() == x.data());
	z = String();
	z += x.substring(0, 5);
	CHECK(z.data() == x.data());
	z += x.substring(0, 5);
	CHECK(z == "abcdeabcde");
	CHECK(z.data() != x.data());
	z = String::make_out_of_memory();
	z += x;
	CHECK(z.out_of_memory());
    }

    StringAccum xx(24);
    xx << "abcdefghijklmn";
    CHECK(xx.capacity() - xx.length() < 12);
    xx.append(xx.data() + 1, 12);
    CHECK(strcmp(xx.c_str(), "abcdefghijklmnbcdefghijklm") == 0);
    xx << xx;
    CHECK(strcmp(xx.c_str(), "abcdefghijklmnbcdefghijklmabcdefghijklmnbcdefghijklm") == 0);
    xx << String::make_out_of_memory();
    CHECK(!xx.out_of_memory());
    xx.assign_out_of_memory();
    xx.append("X", 1);
    CHECK(xx.out_of_memory());
    CHECK(xx.take_string() == String::make_out_of_memory());

    // String hash codes
    {
	static const char hash1[] = "boot_countXXXXXXYboot_countYYYYYZZboot_countZZZZWWWboot_countWWW";
	String s1 = String::make_stable(hash1, 10);
	String s2 = String::make_stable(hash1 + 17, 10);
	String s3 = String::make_stable(hash1 + 34, 10);
	String s4 = String::make_stable(hash1 + 51, 10);
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

    // String confparse
    {
	String s1 = String::make_stable("click-align");
	CHECK(s1.find_left('c') == 0);
	CHECK(s1.find_left('c', 1) == 3);
	CHECK(s1.find_left('c', 3) == 3);
	CHECK(s1.find_left('c', 4) == -1);
	CHECK(s1.find_left("c") == 0);
	CHECK(s1.find_left("c", 1) == 3);
	CHECK(s1.find_left("c", 3) == 3);
	CHECK(s1.find_left("c", 5) == -1);
	CHECK(s1.find_left("li") == 1);
	CHECK(s1.find_left("li", 2) == 7);
	CHECK(s1.find_left("li", 8) == -1);
	CHECK(s1.find_left("", 0) == 0);
	CHECK(s1.find_left("", 10) == 10);
	CHECK(s1.find_left("", 11) == 11);
	CHECK(s1.find_left("", 12) == -1);
	CHECK(s1.find_left("a") == 6);
	CHECK(s1.substring(0, -1).find_left('n') == -1);
    }

#if CLICK_USERLEVEL
    // click_strcmp
    CHECK(click_strcmp("a", "b") < 0);
    CHECK(click_strcmp("a9", "a10") < 0);
    CHECK(click_strcmp("a001", "a2") < 0);   // 1 < 2
    CHECK(click_strcmp("a001", "a1") > 0);   // longer string of initial zeros
    CHECK(click_strcmp("a", "B") < 0);
    CHECK(click_strcmp("Baa", "baa") < 0);
    CHECK(click_strcmp("Baa", "caa") < 0);
    CHECK(click_strcmp("baa", "Caa") < 0);
    CHECK(click_strcmp("baa", "Baa") > 0);
    CHECK(click_strcmp("baA", "baA") == 0);
    CHECK(click_strcmp("a9x", "a10") < 0);
    CHECK(click_strcmp("a9x", "a9xy") < 0);
    CHECK(click_strcmp("0", "0.1") < 0);
    CHECK(click_strcmp("0", "0.") < 0);
    CHECK(click_strcmp("0", "-0.1") > 0);
    CHECK(click_strcmp("-0", "-0.1") > 0);
    CHECK(click_strcmp("-9", "-0.1") < 0);
    CHECK(click_strcmp("9", "0.1") > 0);
    CHECK(click_strcmp("-0.2", "-0.1") < 0);
    CHECK(click_strcmp("-2.2", "-2") < 0);
    CHECK(click_strcmp("2.2", "2") > 0);
    CHECK(click_strcmp("0.2", "0.1") > 0);
    CHECK(click_strcmp("0.2", "0.39") < 0);
    CHECK(click_strcmp(".2", "0.2") < 0);
    CHECK(click_strcmp("a-2", "a-23") < 0);
    CHECK(click_strcmp("a-2", "a-3") < 0);
    CHECK(click_strcmp("a1.2", "a1a") > 0);
    CHECK(click_strcmp("1.2.3.4", "10.2.3.4") < 0);
    CHECK(click_strcmp("1.12", "1.2") < 0);
    CHECK(click_strcmp("1.012", "1.2") < 0);
    CHECK(click_strcmp("1.012.", "1.2.") < 0);
    CHECK(click_strcmp("1.12.3.4", "1.2.3.4") > 0);
    CHECK(click_strcmp("1.2.10.4", "1.2.9.4") > 0);
    CHECK(click_strcmp("1.2.10.4:100", "1.2.10.4:2") > 0);
#endif

    Vector<String> conf;
    conf.push_back("SIZE 123456789");
    size_t test_size;
    CHECK(cp_va_kparse(conf, this, errh,
                       "SIZE", 0, cpSize, &test_size,
                       cpEnd) == 1 && test_size == 123456789);
    CHECK(Args(conf, this, errh)
	  .read("SIZE", test_size).read_status(b)
	  .complete() >= 0 && b == true && test_size == 123456789);

    Vector<String> results;
    bool b2;
    CHECK(Args(this, errh).push_back_args("A 1, B 2, A 3, A 4   , A 5")
	  .read_all_with("A", AnyArg(), results).read_status(b)
	  .read("B", i32).read_status(b2)
	  .complete() >= 0);
    CHECK(b == true);
    CHECK(b2 == true);
    CHECK(i32 == 2);
    CHECK(results.size() == 4);
    CHECK(results[0] == "1" && results[1] == "3" && results[2] == "4" && results[3] == "5");

    results.clear();
    CHECK(Args(this, errh).push_back_args("B 3")
	  .read_all_with("A", AnyArg(), results).read_status(b)
	  .read("B", i32).read_status(b2)
	  .complete() >= 0
	  && b == false && b2 == true && i32 == 3
	  && results.size() == 0);

    {
	CHECK(String(true) + " " + String(false) == "true false");
	StringAccum sa;
	sa << true << ' ' << false;
	CHECK(sa.take_string() == "true false");
    }

    results.clear();
    CHECK(Args(this, errh).push_back_args("A 1, B 2, A 3, A 4, A 5")
	  .read_all("A", AnyArg(), results).read_status(b)
	  .consume() >= 0);
    CHECK(b == true);
    CHECK(results.size() == 4);
    CHECK(results[0] == "1" && results[1] == "3" && results[2] == "4" && results[3] == "5");

    int32_t i32b;
    i32 = i32b = 99;
    CHECK(Args(this, errh).push_back_args("A 1")
          .read_or_set("A", i32, 9).read_status(b)
          .read_or_set("B", i32b, 3).read_status(b2)
          .consume() >= 0);
    CHECK(b == true);
    CHECK(b2 == false);
    CHECK(i32 == 1);
    CHECK(i32b == 3);

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(ConfParseTest)
CLICK_ENDDECLS
