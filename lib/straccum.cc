// -*- c-basic-offset: 4; related-file-name: "../include/click/straccum.hh" -*-
/*
 * straccum.{cc,hh} -- build up strings with operator<<
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/straccum.hh>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/timestamp.hh>
#include <stdarg.h>
CLICK_DECLS

void
StringAccum::make_out_of_memory()
{
    assert(_cap >= 0);
    CLICK_LFREE(_s, _cap);
    _s = reinterpret_cast<unsigned char *>(const_cast<char *>(String::out_of_memory_data()));
    _cap = -1;
    _len = 0;
}

bool
StringAccum::grow(int want)
{
    // can't append to out-of-memory strings
    if (_cap < 0)
	return false;
  
    int ncap = (_cap ? _cap * 2 : 128);
    while (ncap <= want)
	ncap *= 2;
  
    unsigned char *n = (unsigned char *) CLICK_LALLOC(ncap);
    if (!n) {
	make_out_of_memory();
	return false;
    }
  
    if (_s)
	memcpy(n, _s, _cap);
    CLICK_LFREE(_s, _cap);
    _s = n;
    _cap = ncap;
    return true;
}

const char *
StringAccum::c_str()
{
    if (_len < _cap || grow(_len))
	_s[_len] = '\0';
    return reinterpret_cast<char *>(_s);
}

String
StringAccum::take_string()
{
    int len = length();
    if (len) {
	int capacity = _cap;
	return String::claim_string(reinterpret_cast<char *>(take_bytes()), len, capacity);
    } else if (out_of_memory())
	return String::out_of_memory_string();
    else
	return String();
}

void
StringAccum::append_fill(int c, int len)
{
    if (char *s = extend(len))
	memset(s, c, len);
}

void
StringAccum::swap(StringAccum &o)
{
    unsigned char *os = o._s;
    int olen = o._len, ocap = o._cap;
    o._s = _s;
    o._len = _len, o._cap = _cap;
    _s = os;
    _len = olen, _cap = ocap;
}

StringAccum &
operator<<(StringAccum &sa, long i)
{
    if (char *x = sa.reserve(24)) {
	int len = sprintf(x, "%ld", i);
	sa.forward(len);
    }
    return sa;
}

StringAccum &
operator<<(StringAccum &sa, unsigned long u)
{
    if (char *x = sa.reserve(24)) {
	int len = sprintf(x, "%lu", u);
	sa.forward(len);
    }
    return sa;
}

void
StringAccum::append_numeric(String::uint_large_t q, int base, bool uppercase)
{
    // Unparse a large integer. Linux kernel sprintf can't handle %lld, so we
    // provide our own function, and use it everywhere to catch bugs.
  
    char buf[256];
    char *trav = buf + 256;

    assert(base == 10 || base == 16 || base == 8);
    if (base != 10) {
	const char *digits = (uppercase ? "0123456789ABCDEF" : "0123456789abcdef");
	while (q > 0) {
	    *--trav = digits[q & (base - 1)];
	    q >>= (base >> 3) + 2;
	}
    }
  
    while (q > 0) {
	// k = Approx[q/10] -- know that k <= q/10
	String::uint_large_t k = (q >> 4) + (q >> 5) + (q >> 8) + (q >> 9)
	    + (q >> 12) + (q >> 13) + (q >> 16) + (q >> 17);
	String::uint_large_t m;

	// increase k until it exactly equals floor(q/10). on exit, m is
	// the remainder: m < 10 and q == 10*k + m.
	while (1) {
	    // d = 10*k
	    String::uint_large_t d = (k << 3) + (k << 1);
	    m = q - d;
	    if (m < 10)
		break;
	
	    // delta = Approx[m/10] -- know that delta <= m/10
	    String::uint_large_t delta = (m >> 4) + (m >> 5) + (m >> 8) + (m >> 9);
	    if (m >= 0x1000)
		delta += (m >> 12) + (m >> 13) + (m >> 16) + (m >> 17);
	
	    // delta might have underflowed: add at least 1
	    k += (delta ? delta : 1);
	}
      
	*--trav = '0' + (unsigned)m;
	q = k;
    }
  
    // make sure at least one 0 is written
    if (trav == buf + 256)
	*--trav = '0';

    append(trav, buf + 256);
}

void
StringAccum::append_numeric(String::int_large_t q, int base, bool uppercase)
{
    if (q < 0) {
	*this << '-';
	append_numeric(static_cast<String::uint_large_t>(-q), base, uppercase);
    } else
	append_numeric(static_cast<String::uint_large_t>(q), base, uppercase);
}

#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
StringAccum &
operator<<(StringAccum &sa, double d)
{
    if (char *x = sa.reserve(256)) {
	int len = sprintf(x, "%.12g", d);
	sa.forward(len);
    }
    return sa;
}
#endif

StringAccum &
operator<<(StringAccum &sa, void *v)
{
    if (char *x = sa.reserve(30)) {
	int len = sprintf(x, "%p", v);
	sa.forward(len);
    }
    return sa;
}

StringAccum &
StringAccum::snprintf(int n, const char *format, ...)
{
    va_list val;
    va_start(val, format);
    if (char *x = reserve(n + 1)) {
#if CLICK_LINUXMODULE || HAVE_VSNPRINTF
	int len = vsnprintf(x, n + 1, format, val);
#else
	int len = vsprintf(x, format, val);
	assert(len <= n);
#endif
	forward(len);
    }
    va_end(val);
    return *this;
}

CLICK_ENDDECLS
