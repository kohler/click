// -*- c-basic-offset: 4; related-file-name: "../include/click/straccum.hh" -*-
/*
 * straccum.{cc,hh} -- build up strings with operator<<
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/straccum.hh>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/timestamp.hh>
#include <stdarg.h>
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
 * StringAccum object, the object becomes a special "out-of-memory" string.
 * Out-of-memory strings are contagious: the result of any concatenation
 * operation involving an out-of-memory string is another out-of-memory
 * string.  Calling take_string() on an out-of-memory StringAccum returns an
 * out-of-memory String.
 *
 * Out-of-memory StringAccum objects nominally have zero characters.
 */
    

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

/** @brief Append @a len copies of character @a c to this StringAccum. */
void
StringAccum::append_fill(int c, int len)
{
    if (char *s = extend(len))
	memset(s, c, len);
}

/** @brief Null-terminate this StringAccum and return its data.

    Note that the null character does not contribute to the StringAccum's
    length(), and later append() and similar operations can overwrite it.  If
    appending the null character fails, the StringAccum becomes out-of-memory
    and the returned value is a null string. */
const char *
StringAccum::c_str()
{
    if (_len < _cap || grow(_len))
	_s[_len] = '\0';
    return reinterpret_cast<char *>(_s);
}

/** @brief Return a String object with this StringAccum's contents.

    This operation donates the StringAccum's memory to the returned String.
    After a call to take_string(), the StringAccum object becomes empty, and
    any future append() operations may cause memory allocations.  If the
    StringAccum is out-of-memory, the returned String is also out-of-memory.
    However, take_string() resets the StringAccum's out-of-memory state. */
String
StringAccum::take_string()
{
    int len = length();
    int cap = _cap;
    char *str = reinterpret_cast<char *>(_s);
    if (len > 0) {
	_s = 0;
	_len = _cap = 0;
	return String::claim_string(str, len, cap);
    } else if (!out_of_memory())
	return String();
    else {
	clear();
	return String::out_of_memory_string();
    }
}

/** @brief Swaps this StringAccum's contents with @a o. */
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

/** @brief Append representation of @a num to this StringAccum.
    @param  num        number to append
    @param  base       numeric base: must be 8, 10, or 16
    @param  uppercase  true means use uppercase letters in base 16 */
void
StringAccum::append_numeric(String::uint_large_t num, int base, bool uppercase)
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
	String::uint_large_t k = (num >> 4) + (num >> 5) + (num >> 8)
	    + (num >> 9) + (num >> 12) + (num >> 13) + (num >> 16)
	    + (num >> 17);
	String::uint_large_t m;

	// increase k until it exactly equals floor(num/10). on exit, m is
	// the remainder: m < 10 and num == 10*k + m.
	while (1) {
	    // d = 10*k
	    String::uint_large_t d = (k << 3) + (k << 1);
	    m = num - d;
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
	num = k;
    }
  
    // make sure at least one 0 is written
    if (trav == buf + 256)
	*--trav = '0';

    append(trav, buf + 256);
}

/** @overload */
void
StringAccum::append_numeric(String::int_large_t num, int base, bool uppercase)
{
    if (num < 0) {
	*this << '-';
	append_numeric(static_cast<String::uint_large_t>(-num), base, uppercase);
    } else
	append_numeric(static_cast<String::uint_large_t>(num), base, uppercase);
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
    @brief Append hexadecimal representation @a ptr's value to @a sa.
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
    @param  n       maximum number of characters to print
    @param  format  format argument to snprintf()

    The terminating null character is not appended to the string.
    
    @note The safe vsnprintf() variant is called if it exists.  It does in
    the Linux kernel, and on modern Unix variants.  However,
    if it does not exist on your machine, then this function is actually
    unsafe, and you should make sure that the printf() invocation represented
    by your arguments will never write more than @a n characters, not
    including the terminating null. */
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
