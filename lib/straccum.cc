// -*- c-basic-offset: 4; related-file-name: "../include/click/straccum.hh" -*-
/*
 * straccum.{cc,hh} -- build up strings with operator<<
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2007 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
 * Copyright (c) 1999-2012 Eddie Kohler
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
#if CLICK_BSDMODULE
# include <machine/stdarg.h>
#else
# include <stdarg.h>
#endif
CLICK_DECLS

/** @class StringAccum
 * @brief Efficiently build up Strings from pieces.
 *
 * Like the String class, StringAccum represents a string of characters.
 * However, unlike a String, a StringAccum is inherently mutable, and
 * efficiently supports building up a large string from many smaller pieces.
 *
 * StringAccum objects support operator<<() operations for most fundamental
 * data types.  A StringAccum is generally built up by operator<<(), and then
 * turned into a String by the take_string() method.  Extracting the String
 * from a StringAccum does no memory allocation or copying; the StringAccum's
 * memory is donated to the String.
 *
 * <h3>Out-of-memory StringAccums</h3>
 *
 * When there is not enough memory to add requested characters to a
 * StringAccum object, the object becomes a special "out-of-memory"
 * StringAccum. Out-of-memory objects are contagious: the result of any
 * concatenation operation involving an out-of-memory StringAccum is another
 * out-of-memory StringAccum. Calling take_string() on an out-of-memory
 * StringAccum returns an out-of-memory String.
 *
 * Note that appending an out-of-memory String to a StringAccum <em>does
 * not</em> make the StringAccum out-of-memory.
 */

/** @brief Change this StringAccum into an out-of-memory StringAccum. */
void
StringAccum::assign_out_of_memory()
{
    if (r_.cap > 0)
	CLICK_LFREE(r_.s - MEMO_SPACE, r_.cap + MEMO_SPACE);
    r_.s = reinterpret_cast<unsigned char *>(const_cast<char *>(String::empty_data()));
    r_.cap = -1;
    r_.len = 0;
}

char *
StringAccum::grow(int want)
{
    // can't append to out-of-memory strings
    if (r_.cap < 0)
	return 0;

    int ncap = (r_.cap ? (r_.cap + MEMO_SPACE) * 2 : 128) - MEMO_SPACE;
    while (ncap <= want)
	ncap = (ncap + MEMO_SPACE) * 2 - MEMO_SPACE;

    unsigned char *n = (unsigned char *) CLICK_LALLOC(ncap + MEMO_SPACE);
    if (!n) {
	assign_out_of_memory();
	return 0;
    }
    n += MEMO_SPACE;

    if (r_.cap > 0) {
	memcpy(n, r_.s, r_.len);
	CLICK_LFREE(r_.s - MEMO_SPACE, r_.cap + MEMO_SPACE);
    }
    r_.s = n;
    r_.cap = ncap;
    return reinterpret_cast<char *>(r_.s + r_.len);
}

/** @brief Set the StringAccum's length to @a len.
    @pre @a len >= 0
    @return 0 on success, -ENOMEM on failure */
int
StringAccum::resize(int len)
{
    assert(len >= 0);
    if (len > r_.cap && !grow(len))
	return -ENOMEM;
    else {
	r_.len = len;
	return 0;
    }
}

char *
StringAccum::hard_extend(int nadjust, int nreserve)
{
    char *x;
    if (r_.len + nadjust + nreserve <= r_.cap)
	x = reinterpret_cast<char *>(r_.s + r_.len);
    else
	x = grow(r_.len + nadjust + nreserve);
    if (x)
	r_.len += nadjust;
    return x;
}

/** @brief Null-terminate this StringAccum and return its data.

    Note that the null character does not contribute to the StringAccum's
    length(), and later append() and similar operations can overwrite it. If
    appending the null character fails, the StringAccum becomes
    out-of-memory and the returned value is a null string. */
const char *
StringAccum::c_str()
{
    if (r_.len < r_.cap || grow(r_.len))
	r_.s[r_.len] = '\0';
    return reinterpret_cast<char *>(r_.s);
}

/** @brief Append @a len copies of character @a c to the StringAccum. */
void
StringAccum::append_fill(int c, int len)
{
    if (char *s = extend(len))
	memset(s, c, len);
}

void
StringAccum::hard_append(const char *s, int len)
{
    // We must be careful about calls like "sa.append(sa.begin(), sa.end())";
    // a naive implementation might use sa's data after freeing it.
    const char *my_s = reinterpret_cast<char *>(r_.s);

    if (r_.len + len <= r_.cap) {
    success:
	memcpy(r_.s + r_.len, s, len);
	r_.len += len;
    } else if (likely(s < my_s || s >= my_s + r_.cap)) {
	if (grow(r_.len + len))
	    goto success;
    } else {
	rep_t old_r = r_;
	r_ = rep_t();
	if (char *new_s = extend(old_r.len + len)) {
	    memcpy(new_s, old_r.s, old_r.len);
	    memcpy(new_s + old_r.len, s, len);
	}
	CLICK_LFREE(old_r.s - MEMO_SPACE, old_r.cap + MEMO_SPACE);
    }
}

void
StringAccum::hard_append_cstr(const char *cstr)
{
    append(cstr, strlen(cstr));
}

bool
StringAccum::append_utf8_hard(int ch)
{
    if (ch < 0x8000) {
	append(static_cast<char>(0xC0 | (ch >> 6)));
	goto char1;
    } else if (ch < 0x10000) {
	if (unlikely((ch >= 0xD800 && ch < 0xE000) || ch > 0xFFFD))
	    return false;
	append(static_cast<char>(0xE0 | (ch >> 12)));
	goto char2;
    } else if (ch < 0x110000) {
	append(static_cast<char>(0xF0 | (ch >> 18)));
	append(static_cast<char>(0x80 | ((ch >> 12) & 0x3F)));
    char2:
	append(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
    char1:
	append(static_cast<char>(0x80 | (ch & 0x3F)));
    } else
	return false;
    return true;
}

/** @brief Return a String object with this StringAccum's contents.

    This operation donates the StringAccum's memory to the returned String.
    After a call to take_string(), the StringAccum object becomes empty, and
    any future append() operations may cause memory allocations. If the
    StringAccum is out-of-memory, the returned String is also out-of-memory,
    but the StringAccum's out-of-memory state is reset. */
String
StringAccum::take_string()
{
    int len = length();
    int cap = r_.cap;
    char *str = reinterpret_cast<char *>(r_.s);
    if (len > 0 && cap > 0) {
	r_ = rep_t();
	return String::make_claim(str, len, cap);
    } else if (!out_of_memory())
	return String();
    else {
	clear();
	return String::make_out_of_memory();
    }
}

/** @brief Swap this StringAccum's contents with @a x. */
void
StringAccum::swap(StringAccum &x)
{
    rep_t xr = x.r_;
    x.r_ = r_;
    r_ = xr;
}

/** @relates StringAccum
    @brief Append decimal representation of @a i to @a sa.
    @return @a sa */
StringAccum &
operator<<(StringAccum &sa, long i)
{
    if (char *x = sa.reserve(24)) {
	int len = sprintf(x, "%ld", i);
	sa.adjust_length(len);
    }
    return sa;
}

/** @relates StringAccum
    @brief Append decimal representation of @a u to @a sa.
    @return @a sa */
StringAccum &
operator<<(StringAccum &sa, unsigned long u)
{
    if (char *x = sa.reserve(24)) {
	int len = sprintf(x, "%lu", u);
	sa.adjust_length(len);
    }
    return sa;
}

/** @overload */
void
StringAccum::append_numeric(String::uintmax_t num, int base, bool uppercase)
{
    // Unparse a large integer. Linux kernel sprintf can't handle %lld, so we
    // provide our own function, and use it everywhere to catch bugs.

    char buf[256];
    char *trav = buf + 256;

    assert(base == 10 || base == 16 || base == 8);
    if (base != 10) {
	const char *digits = (uppercase ? "0123456789ABCDEF" : "0123456789abcdef");
	while (num > 0) {
	    *--trav = digits[num & (base - 1)];
	    num >>= (base >> 3) + 2;
	}
    }

    while (num > 0) {
	// k = Approx[num/10] -- know that k <= num/10
	String::uintmax_t k = (num >> 4) + (num >> 5) + (num >> 8)
	    + (num >> 9) + (num >> 12) + (num >> 13) + (num >> 16)
	    + (num >> 17);
	String::uintmax_t m;

	// increase k until it exactly equals floor(num/10). on exit, m is
	// the remainder: m < 10 and num == 10*k + m.
	while (1) {
	    // d = 10*k
	    String::uintmax_t d = (k << 3) + (k << 1);
	    m = num - d;
	    if (m < 10)
		break;

	    // delta = Approx[m/10] -- know that delta <= m/10
	    String::uintmax_t delta = (m >> 4) + (m >> 5) + (m >> 8) + (m >> 9);
	    if (m >= 0x1000)
		delta += (m >> 12) + (m >> 13) + (m >> 16) + (m >> 17);

	    // delta might have underflowed: add at least 1
	    k += (delta ? delta : 1);
	}

	*--trav = '0' + (unsigned)m;
	num = k;
    }

    // make sure at least one 0 is written
    if (trav == buf + 256)
	*--trav = '0';

    append(trav, buf + 256);
}

/** @brief Append string representation of @a x to this StringAccum.
    @param x number to append
    @param base numeric base: must be 8, 10, or 16
    @param uppercase true means use uppercase letters in base 16 */
void
StringAccum::append_numeric(String::intmax_t num, int base, bool uppercase)
{
    if (num < 0) {
	*this << '-';
	append_numeric(static_cast<String::uintmax_t>(-num), base, uppercase);
    } else
	append_numeric(static_cast<String::uintmax_t>(num), base, uppercase);
}

#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
/** @relates StringAccum
    @brief Append decimal representation of @a d to @a sa.
    @return @a sa */
StringAccum &
operator<<(StringAccum &sa, double d)
{
    if (char *x = sa.reserve(256)) {
	int len = sprintf(x, "%.12g", d);
	sa.adjust_length(len);
    }
    return sa;
}
#endif

/** @relates StringAccum
    @brief Append hexadecimal representation of @a ptr's value to @a sa.
    @return @a sa */
StringAccum &
operator<<(StringAccum &sa, void *ptr)
{
    if (char *x = sa.reserve(30)) {
	int len = sprintf(x, "%p", ptr);
	sa.adjust_length(len);
    }
    return sa;
}

/** @brief Append result of snprintf() to this StringAccum.
    @param n maximum number of characters to print
    @param format format argument to snprintf()
    @return *this

    The terminating null character is not appended to the string.

    @note The safe vsnprintf() variant is called if it exists. It does in
    the Linux kernel, and on modern Unix variants. However, if it does not
    exist on your machine, then this function is actually unsafe, and you
    should make sure that the printf() invocation represented by your
    arguments will never write more than @a n characters, not including the
    terminating null. */
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
	adjust_length(len);
    }
    va_end(val);
    return *this;
}

CLICK_ENDDECLS
