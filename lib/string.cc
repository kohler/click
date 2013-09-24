// -*- c-basic-offset: 4; related-file-name: "../include/click/string.hh" -*-
/*
 * string.{cc,hh} -- a String class with shared substrings
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2004-2007 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc.
 * Copyright (c) 2012 Eddie Kohler
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
#include <click/string.hh>
#include <click/straccum.hh>
#include <click/glue.hh>
CLICK_DECLS

/** @file string.hh
 * @brief Click's String class.
 */

/** @class String
 * @brief A string of characters.
 *
 * The String class represents a string of characters.  Strings may be
 * constructed from C strings, characters, numbers, and so forth.  They may
 * also be added together.  The underlying character arrays are dynamically
 * allocated; String operations allocate and free memory as needed.  A String
 * and its substrings generally share memory.  Accessing a character by index
 * takes O(1) time; so does creating a substring.
 *
 * <h3>Out-of-memory strings</h3>
 *
 * When there is not enough memory to create a particular string, a special
 * "out-of-memory" string is returned instead. Out-of-memory strings are
 * contagious: the result of any concatenation operation involving an
 * out-of-memory string is another out-of-memory string. Thus, the final
 * result of a series of String operations will be an out-of-memory string,
 * even if the out-of-memory condition occurs in the middle.
 *
 * The canonical out-of-memory string is 14 bytes long, and equals the UTF-8
 * encoding of "\U0001F4A3ENOMEM\U0001F4A3" (that is, U+1F4A3 BOMB +
 * "ENOMEM" + U+1F4A3 BOMB). This sequence is unlikely to show up in normal
 * text, compares high relative to most other textual strings, and is valid
 * UTF-8.
 *
 * All canonical out-of-memory strings are equal and share the same data(),
 * which is different from the data() of any other string. See
 * String::out_of_memory_data(). The String::make_out_of_memory() function
 * returns a canonical out-of-memory string.
 *
 * Other strings may also be out-of-memory strings. For example,
 * String::make_stable(String::out_of_memory_data()) ==
 * String::make_out_of_memory(), and some (but not all) substrings of
 * out-of-memory strings are also out-of-memory strings.
 */

const char String::null_data = '\0';
// oom_data is the UTF-8 encoding of U+1F4A3 BOMB + "ENOMEM" + U+1F4A3 BOMB
const char String::oom_data[] = "\360\237\222\243ENOMEM\360\237\222\243";
const char String::bool_data[] = "false\0true";
const char String::int_data[] = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009";

#if HAVE_STRING_PROFILING > 1
# define MEMO_INITIALIZER_TAIL , 0, 0
#else
# define MEMO_INITIALIZER_TAIL
#endif

const String::rep_t String::null_string_rep = {
    &null_data, 0, 0
};
const String::rep_t String::oom_string_rep = {
    oom_data, oom_len, 0
};

#if HAVE_STRING_PROFILING
uint64_t String::live_memo_count;
uint64_t String::memo_sizes[55];
uint64_t String::live_memo_sizes[55];
uint64_t String::live_memo_bytes[55];
# if HAVE_STRING_PROFILING > 1
String::memo_t *String::live_memos[55];
# endif
#endif

/** @cond never */
String::memo_t *
String::create_memo(char *space, int dirty, int capacity)
{
    assert(capacity > 0 && capacity >= dirty);
    memo_t *memo;
    if (space)
	memo = reinterpret_cast<memo_t *>(space);
    else
	memo = (memo_t *) CLICK_LALLOC(MEMO_SPACE + capacity);
    if (memo) {
	memo->capacity = capacity;
	memo->dirty = dirty;
	memo->refcount = (space ? 0 : 1);
#if HAVE_STRING_PROFILING
	int bucket = profile_memo_size_bucket(dirty, capacity);
	++memo_sizes[bucket];
	++live_memo_sizes[bucket];
	live_memo_bytes[bucket] += capacity;
	++live_memo_count;
# if HAVE_STRING_PROFILING > 1
	memo->pprev = &live_memos[bucket];
	if ((memo->next = *memo->pprev))
	    memo->next->pprev = &memo->next;
	*memo->pprev = memo;
# endif
#endif
    }
    return memo;
}

void
String::delete_memo(memo_t *memo)
{
    assert(!memo->refcount);
    assert(memo->capacity > 0);
    assert(memo->capacity >= memo->dirty);
#if HAVE_STRING_PROFILING
    int bucket = profile_memo_size_bucket(memo->dirty, memo->capacity);
    --live_memo_sizes[bucket];
    live_memo_bytes[bucket] -= memo->capacity;
    --live_memo_count;
# if HAVE_STRING_PROFILING > 1
    if ((*memo->pprev = memo->next))
	memo->next->pprev = memo->pprev;
# endif
#endif
    CLICK_LFREE(memo, MEMO_SPACE + memo->capacity);
}


#if HAVE_STRING_PROFILING
void
String::one_profile_report(StringAccum &sa, int i, int examples)
{
    if (i <= 16)
	sa << "memo_dirty_" << i;
    else if (i < 25) {
	uint32_t s = (i - 17) * 2 + 17;
	sa << "memo_cap_" << s << '_' << (s + 1);
    } else if (i < 29) {
	uint32_t s = (i - 25) * 8 + 33;
	sa << "memo_cap_" << s << '_' << (s + 7);
    } else {
	uint32_t s1 = (1U << (i - 23)) + 1;
	uint32_t s2 = (s1 - 1) << 1;
	sa << "memo_cap_" << s1 << '_' << s2;
    }
    sa << '\t' << live_memo_sizes[i] << '\t' << memo_sizes[i] << '\t' << live_memo_bytes[i] << '\n';
    if (examples) {
# if HAVE_STRING_PROFILING > 1
	for (memo_t *m = live_memos[i]; m; m = m->next) {
	    sa << "    [" << m->dirty << "] ";
	    uint32_t dirty = m->dirty;
	    if (dirty > 0 && m->real_data[dirty - 1] == '\0')
		--dirty;
	    sa.append(m->real_data, dirty > 128 ? 128 : dirty);
	    sa << '\n';
	}
# endif
    }
}

void
String::profile_report(StringAccum &sa, int examples)
{
    uint64_t all_live_sizes = 0, all_sizes = 0, all_live_bytes = 0;
    for (int i = 0; i < 55; ++i) {
	if (memo_sizes[i])
	    one_profile_report(sa, i, examples);
	all_live_sizes += live_memo_sizes[i];
	all_sizes += memo_sizes[i];
	all_live_bytes += live_memo_bytes[i];
    }
    sa << "memo_total\t" << all_live_sizes << '\t' << all_sizes << '\t' << all_live_bytes << '\n';
}
#endif

/** @endcond never */


/** @brief Construct a base-10 string representation of @a x. */
String::String(int x)
{
    if (x >= 0 && x < 10)
	assign_memo(int_data + 2 * x, 1, 0);
    else {
	char buf[128];
	sprintf(buf, "%d", x);
	assign(buf, -1, false);
    }
}

/** @overload */
String::String(unsigned x)
{
    if (x < 10)
	assign_memo(int_data + 2 * x, 1, 0);
    else {
	char buf[128];
	sprintf(buf, "%u", x);
	assign(buf, -1, false);
    }
}

/** @overload */
String::String(long x)
{
    if (x >= 0 && x < 10)
	assign_memo(int_data + 2 * x, 1, 0);
    else {
	char buf[128];
	sprintf(buf, "%ld", x);
	assign(buf, -1, false);
    }
}

/** @overload */
String::String(unsigned long x)
{
    if (x < 10)
	assign_memo(int_data + 2 * x, 1, 0);
    else {
	char buf[128];
	sprintf(buf, "%lu", x);
	assign(buf, -1, false);
    }
}

// Implemented a [u]int64_t converter in StringAccum
// (use the code even at user level to hunt out bugs)

#if HAVE_LONG_LONG
/** @overload */
String::String(long long x)
{
    if (x >= 0 && x < 10)
	assign_memo(int_data + 2 * x, 1, 0);
    else {
	StringAccum sa;
	sa << x;
	assign(sa.take_string());
    }
}

/** @overload */
String::String(unsigned long long x)
{
    if (x < 10)
	assign_memo(int_data + 2 * x, 1, 0);
    else {
	StringAccum sa;
	sa << x;
	assign(sa.take_string());
    }
}
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
/** @overload */
String::String(int64_t x)
{
    if (x >= 0 && x < 10)
	assign_memo(int_data + 2 * x, 1, 0);
    else {
	StringAccum sa;
	sa << x;
	assign(sa.take_string());
    }
}

/** @overload */
String::String(uint64_t x)
{
    if (x < 10)
	assign_memo(int_data + 2 * x, 1, 0);
    else {
	StringAccum sa;
	sa << x;
	assign(sa.take_string());
    }
}
#endif

#if HAVE_FLOAT_TYPES
/** @brief Construct a base-10 string representation of @a x.
 * @note This function is only available at user level. */
String::String(double x)
{
    char buf[128];
    int len = sprintf(buf, "%.12g", x);
    assign(buf, len, false);
}
#endif

String
String::hard_make_stable(const char *s, int len)
{
    if (len < 0)
	len = strlen(s);
    return String(s, len, 0);
}

String
String::make_claim(char *str, int len, int capacity)
{
    assert(str && len > 0 && capacity >= len);
    memo_t *new_memo = create_memo(str - MEMO_SPACE, len, capacity);
    return String(str, len, new_memo);
}

/** @brief Create and return a string representation of @a x.
    @param x number
    @param base base; must be 8, 10, or 16, defaults to 10
    @param uppercase if true, then use uppercase letters in base 16 */
String
String::make_numeric(intmax_t num, int base, bool uppercase)
{
    StringAccum sa;
    sa.append_numeric(num, base, uppercase);
    return sa.take_string();
}

/** @overload */
String
String::make_numeric(uintmax_t num, int base, bool uppercase)
{
    StringAccum sa;
    sa.append_numeric(num, base, uppercase);
    return sa.take_string();
}

void
String::assign_out_of_memory()
{
    if (_r.memo)
	deref();
    _r = oom_string_rep;
}

void
String::assign(const char *str, int len, bool need_deref)
{
    if (!str) {
	assert(len <= 0);
	len = 0;
    } else if (len < 0)
	len = strlen(str);

    // need to start with dereference
    if (need_deref) {
	if (unlikely(_r.memo
		     && str >= _r.memo->real_data
		     && str + len <= _r.memo->real_data + _r.memo->capacity)) {
	    // Be careful about "String s = ...; s = s.c_str();"
	    _r.data = str;
	    _r.length = len;
	    return;
	} else
	    deref();
    }

    if (len == 0) {
	_r.memo = 0;
	_r.data = &null_data;

    } else {
	// Make the memo a multiple of 16 characters and bigger than 'len'.
	int memo_capacity = (len + 15 + MEMO_SPACE) & ~15;
	_r.memo = create_memo(0, len, memo_capacity - MEMO_SPACE);
	if (!_r.memo) {
	    assign_out_of_memory();
	    return;
	}
	memcpy(_r.memo->real_data, str, len);
	_r.data = _r.memo->real_data;
    }

    _r.length = len;
}

/** @brief Append @a len unknown characters to this string.
 * @return Modifiable pointer to the appended characters.
 *
 * The caller may safely modify the returned memory. Null is returned if
 * the string becomes out-of-memory. */
char *
String::append_uninitialized(int len)
{
    // Appending anything to "out of memory" leaves it as "out of memory"
    if (len <= 0 || out_of_memory())
	return 0;

    // If we can, append into unused space. First, we check that there's
    // enough unused space for 'len' characters to fit; then, we check
    // that the unused space immediately follows the data in '*this'.
    uint32_t dirty;
    if (_r.memo
	&& ((dirty = _r.memo->dirty), _r.memo->capacity > dirty + len)) {
	char *real_dirty = _r.memo->real_data + dirty;
	if (real_dirty == _r.data + _r.length
	    && atomic_uint32_t::compare_swap(_r.memo->dirty, dirty, dirty + len) == dirty) {
	    _r.length += len;
	    assert(_r.memo->dirty < _r.memo->capacity);
#if HAVE_STRING_PROFILING
	    profile_update_memo_dirty(_r.memo, dirty, dirty + len, _r.memo->capacity);
#endif
	    return real_dirty;
	}
    }

    // Now we have to make new space. Make sure the memo is a multiple of 16
    // bytes and that it is at least 16. But for large strings, allocate a
    // power of 2, since power-of-2 sizes minimize waste in frequently-used
    // allocators, like Linux kmalloc.
    int want_memo_len = _r.length + len + MEMO_SPACE;
    int memo_capacity;
    if (want_memo_len <= 1024)
	memo_capacity = (want_memo_len + 15) & ~15;
    else
	for (memo_capacity = 2048; memo_capacity < want_memo_len; )
	    memo_capacity *= 2;

#if CLICK_DMALLOC
    // Keep total allocation a power of 2 by leaving extra space for the
    // DMALLOC Chunk.
    if (want_memo_len < memo_capacity - 32)
	memo_capacity -= 32;
#endif

    memo_t *new_memo = create_memo(0, _r.length + len, memo_capacity - MEMO_SPACE);
    if (!new_memo) {
	assign_out_of_memory();
	return 0;
    }

    char *new_data = new_memo->real_data;
    memcpy(new_data, _r.data, _r.length);

    deref();
    _r.data = new_data;
    new_data += _r.length;	// now new_data points to the garbage
    _r.length += len;
    _r.memo = new_memo;
    return new_data;
}

void
String::append(const char *s, int len, memo_t *memo)
{
    if (!s) {
	assert(len <= 0);
	len = 0;
    } else if (len < 0)
	len = strlen(s);

    if (unlikely(len == 0) || out_of_memory())
	/* do nothing */;
    else if (unlikely(s == out_of_memory_data()) && !memo)
	// Appending "out of memory" to a regular string makes it "out of
	// memory"
	assign_out_of_memory();
    else if (_r.length == 0 && reinterpret_cast<uintptr_t>(memo) > 1) {
	deref();
	assign_memo(s, len, memo);
    } else if (likely(!(_r.memo
			&& s >= _r.memo->real_data
			&& s + len <= _r.memo->real_data + _r.memo->capacity))) {
	if (char *space = append_uninitialized(len))
	    memcpy(space, s, len);
    } else {
	String preserve_s(*this);
	if (char *space = append_uninitialized(len))
	    memcpy(space, s, len);
    }
}

/** @brief Append @a len copies of character @a c to this string. */
void
String::append_fill(int c, int len)
{
    assert(len >= 0);
    if (char *space = append_uninitialized(len))
	memset(space, c, len);
}

/** @brief Ensure the string's data is unshared and return a mutable
    pointer to it. */
char *
String::mutable_data()
{
    // If _memo has a capacity (it's not one of the special strings) and it's
    // uniquely referenced, return _data right away.
    if (_r.memo && _r.memo->refcount == 1)
	return const_cast<char *>(_r.data);

    // Otherwise, make a copy of it. Rely on: deref() doesn't change _data or
    // _length; and if _capacity == 0, then deref() doesn't free _real_data.
    assert(!_r.memo || _r.memo->refcount > 1);
    // But in multithreaded situations we must hold a local copy of memo!
    String copy(*this);
    deref(); // _r is now invalid
    assign(copy.data(), copy.length(), false);
    return const_cast<char *>(_r.data);
}

const char *
String::hard_c_str() const
{
    // See also c_str().
    // We may already have a '\0' in the right place.  If _memo has no
    // capacity, then this is one of the special strings (null or
    // stable). We are guaranteed, in these strings, that _data[_length]
    // exists. Otherwise must check that _data[_length] exists.
    const char *end_data = _r.data + _r.length;
    if ((_r.memo && end_data >= _r.memo->real_data + _r.memo->dirty)
	|| *end_data != '\0') {
	if (char *x = const_cast<String *>(this)->append_uninitialized(1)) {
	    *x = '\0';
	    --_r.length;
	}
    }
    return _r.data;
}

/** @brief Null-terminate the string and return a mutable pointer to its
    data.
    @sa String::c_str */
char *
String::mutable_c_str()
{
    (void) mutable_data();
    (void) c_str();
    return const_cast<char *>(_r.data);
}

/** @brief Return a substring of this string, consisting of the @a len
    characters starting at index @a pos.
    @param pos substring's first position relative to the string
    @param len length of substring

    If @a pos is negative, starts that far from the end of the string. If @a
    len is negative, leaves that many characters off the end of the string.
    If @a pos and @a len specify a substring that is partly outside the
    string, only the part within the string is returned. If the substring is
    beyond either end of the string, returns an empty string (but this
    should be considered a programming error; a future version may generate
    a warning for this case).

    @note String::substring() is intended to behave like Perl's substr(). */
String
String::substring(int pos, int len) const
{
    if (pos < 0)
	pos += _r.length;

    int pos2;
    if (len < 0)
	pos2 = _r.length + len;
    else if (pos >= 0 && len >= _r.length) // avoid integer overflow
	pos2 = _r.length;
    else
	pos2 = pos + len;

    if (pos < 0)
	pos = 0;
    if (pos2 > _r.length)
	pos2 = _r.length;

    if (pos >= pos2)
	return String();
    else
	return String(_r.data + pos, pos2 - pos, _r.memo);
}

/** @brief Search for a character in a string.
 * @param c character to search for
 * @param start initial search position
 *
 * Return the index of the leftmost occurence of @a c, starting at index
 * @a start and working up to the end of the string.  Returns -1 if @a c
 * is not found. */
int
String::find_left(char c, int start) const
{
    if (start < 0)
	start = 0;
    if (start < _r.length) {
	const char *x = (const char *)
	    memchr(_r.data + start, c, _r.length - start);
	if (x)
	    return x - _r.data;
    }
    return -1;
}

/** @brief Search for a substring in a string.
 * @param x substring to search for
 * @param start initial search position
 *
 * Return the index of the leftmost occurence of the substring @a str,
 * starting at index @a start and working up to the end of the string.
 * Returns -1 if @a str is not found. */
int
String::find_left(const String &x, int start) const
{
    if (start < 0)
	start = 0;
    if (x.length() == 0 && start <= length())
	return start;
    if (start + x.length() > length())
	return -1;
    const char *pos = _r.data + start;
    const char *end_pos = _r.data + length() - x.length() + 1;
    char first_c = (unsigned char) x[0];
    while (pos < end_pos) {
	pos = (const char *) memchr(pos, first_c, end_pos - pos);
	if (!pos)
	    break;
	if (memcmp(pos + 1, x.data() + 1, x.length() - 1) == 0)
	    return pos - _r.data;
	++pos;
    }
    return -1;
}

/** @brief Search for a character in a string.
 * @param c character to search for
 * @param start initial search position
 *
 * Return the index of the rightmost occurence of the character @a c,
 * starting at index @a start and working back to the beginning of the
 * string.  Returns -1 if @a c is not found.  @a start may start beyond
 * the end of the string. */
int
String::find_right(char c, int start) const
{
    if (start >= _r.length)
	start = _r.length - 1;
    for (int i = start; i >= 0; i--)
	if (_r.data[i] == c)
	    return i;
    return -1;
}

static String
hard_lower(const String &s, int pos)
{
    String new_s(s.data(), s.length());
    char *x = const_cast<char *>(new_s.data()); // know it's mutable
    int len = s.length();
    for (; pos < len; pos++)
	x[pos] = tolower((unsigned char) x[pos]);
    return new_s;
}

/** @brief Return a lowercased version of this string.

    Translates the ASCII characters 'A' through 'Z' into their lowercase
    equivalents. */
String
String::lower() const
{
    // avoid copies
    if (!out_of_memory())
	for (int i = 0; i < _r.length; i++)
	    if (_r.data[i] >= 'A' && _r.data[i] <= 'Z')
		return hard_lower(*this, i);
    return *this;
}

static String
hard_upper(const String &s, int pos)
{
    String new_s(s.data(), s.length());
    char *x = const_cast<char *>(new_s.data()); // know it's mutable
    int len = s.length();
    for (; pos < len; pos++)
	x[pos] = toupper((unsigned char) x[pos]);
    return new_s;
}

/** @brief Return an uppercased version of this string.

    Translates the ASCII characters 'a' through 'z' into their uppercase
    equivalents. */
String
String::upper() const
{
    // avoid copies
    for (int i = 0; i < _r.length; i++)
	if (_r.data[i] >= 'a' && _r.data[i] <= 'z')
	    return hard_upper(*this, i);
    return *this;
}

static String
hard_printable(const String &s, int pos)
{
    StringAccum sa(s.length() * 2);
    sa.append(s.data(), pos);
    const unsigned char *x = reinterpret_cast<const unsigned char *>(s.data());
    int len = s.length();
    for (; pos < len; pos++) {
	if (x[pos] >= 32 && x[pos] < 127)
	    sa << x[pos];
	else if (x[pos] < 32)
	    sa << '^' << (unsigned char)(x[pos] + 64);
	else if (char *buf = sa.extend(4, 1))
	    sprintf(buf, "\\%03o", x[pos]);
    }
    return sa.take_string();
}

/** @brief Return a "printable" version of this string.

    Translates control characters 0-31 into "control" sequences, such as
    "^@" for the null character, and characters 127-255 into octal escape
    sequences, such as "\377" for 255. */
String
String::printable() const
{
    // avoid copies
    if (!out_of_memory())
	for (int i = 0; i < _r.length; i++)
	    if (_r.data[i] < 32 || _r.data[i] > 126)
		return hard_printable(*this, i);
    return *this;
}

/** @brief Return this string's contents encoded for JSON.
    @pre *this is encoded in UTF-8.

    For instance, String("a\"").encode_json() == "a\\\"". Note that the
    double-quote characters that usually surround a JSON string are not
    included. */
String
String::encode_json() const
{
    StringAccum sa;
    const char *last = begin(), *end = this->end();
    for (const char *s = last; s != end; ++s) {
	int c = (unsigned char) *s;

	// U+2028 and U+2029 can't appear in Javascript strings! (Though
	// they are legal in JSON strings, according to the JSON
	// definition.)
	if (unlikely(c == 0xE2)
	    && s + 2 < end && (unsigned char) s[1] == 0x80
	    && (unsigned char) (s[2] | 1) == 0xA9)
	    c = 0x2028 + (s[2] & 1);
	else if (likely(c >= 32 && c != '\\' && c != '\"' && c != '/'))
	    continue;

	if (!sa.length())
	    sa.reserve(length() + 16);
	sa.append(last, s);
	sa << '\\';
	switch (c) {
	case '\b':
	    sa << 'b';
	    break;
	case '\f':
	    sa << 'f';
	    break;
	case '\n':
	    sa << 'n';
	    break;
	case '\r':
	    sa << 'r';
	    break;
	case '\t':
	    sa << 't';
	    break;
	case '\\':
	case '\"':
	case '/':
	    sa.append((char) c);
	    break;
	default: // c is a control character, 0x2028, or 0x2029
	    sa.snprintf(5, "u%04X", c);
	    if (c > 255)	// skip rest of encoding of U+202[89]
		s += 2;
	    break;
	}
	last = s + 1;
    }
    if (sa.length()) {
	sa.append(last, end);
	return sa.take_string();
    } else
	return *this;
}

/** @brief Return a substring with spaces trimmed from the end. */
String
String::trim_space() const
{
    for (int i = _r.length - 1; i >= 0; i--)
	if (!isspace((unsigned char) _r.data[i]))
	    return substring(0, i + 1);
    return String();
}

/** @brief Return a hex-quoted version of the string.

    For example, the string "Abcd" would convert to "\<41626364>". */
String
String::quoted_hex() const
{
    static const char hex_digits[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    StringAccum sa;
    char *buf;
    if (out_of_memory() || !(buf = sa.extend(length() * 2 + 3)))
	return make_out_of_memory();
    *buf++ = '\\';
    *buf++ = '<';
    const uint8_t *e = reinterpret_cast<const uint8_t*>(end());
    for (const uint8_t *x = reinterpret_cast<const uint8_t*>(begin()); x < e; x++) {
	*buf++ = hex_digits[(*x >> 4) & 0xF];
	*buf++ = hex_digits[*x & 0xF];
    }
    *buf++ = '>';
    return sa.take_string();
}

/** @brief Return a 32-bit hash function of the characters in [first, last).
 *
 * Uses Paul Hsieh's "SuperFastHash" algorithm, described at
 * http://www.azillionmonkeys.com/qed/hash.html
 * This hash function uses all characters in the string.
 *
 * @invariant If last1 - first1 == last2 - first2 and memcmp(first1, first2,
 * last1 - first1) == 0, then hashcode(first1, last1) == hashcode(first2,
 * last2). */
uint32_t
String::hashcode(const char *first, const char *last)
{
    if (last <= first)
	return 0;

    uint32_t hash = last - first;
    int rem = hash & 3;
    last -= rem;
    uint32_t last16;

#if !HAVE_INDIFFERENT_ALIGNMENT
    if (!(reinterpret_cast<uintptr_t>(first) & 1)) {
#endif
#define get16(p) (*reinterpret_cast<const uint16_t *>((p)))
	for (; first != last; first += 4) {
	    hash += get16(first);
	    uint32_t tmp = (get16(first + 2) << 11) ^ hash;
	    hash = (hash << 16) ^ tmp;
	    hash += hash >> 11;
	}
	if (rem >= 2) {
	    last16 = get16(first);
	    goto rem2;
	}
#undef get16
#if !HAVE_INDIFFERENT_ALIGNMENT
    } else {
# if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
#  define get16(p) (((unsigned char) (p)[0] << 8) + (unsigned char) (p)[1])
# elif CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
#  define get16(p) ((unsigned char) (p)[0] + ((unsigned char) (p)[1] << 8))
# else
#  error "unknown CLICK_BYTE_ORDER"
# endif
	// should be exactly the same as the code above
	for (; first != last; first += 4) {
	    hash += get16(first);
	    uint32_t tmp = (get16(first + 2) << 11) ^ hash;
	    hash = (hash << 16) ^ tmp;
	    hash += hash >> 11;
	}
	if (rem >= 2) {
	    last16 = get16(first);
	    goto rem2;
	}
# undef get16
    }
#endif

    /* Handle end cases */
    if (0) {			// weird organization avoids uninitialized
      rem2:			// variable warnings
	if (rem == 3) {
	    hash += last16;
	    hash ^= hash << 16;
	    hash ^= ((unsigned char) first[2]) << 18;
	    hash += hash >> 11;
	} else {
	    hash += last16;
	    hash ^= hash << 11;
	    hash += hash >> 17;
	}
    } else if (rem == 1) {
	hash += (unsigned char) *first;
	hash ^= hash << 10;
	hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

bool
String::hard_equals(const char *s, int len) const
{
    // It'd be nice to make "out-of-memory" strings compare unequal to
    // anything, even themselves, but this would be a bad idea for Strings
    // used as (for example) keys in hashtables. Instead, "out-of-memory"
    // strings compare unequal to other null strings, but equal to each other.
    if (len < 0)
	len = strlen(s);
    return length() == len && (data() == s || memcmp(data(), s, len) == 0);
}

/** @brief Return true iff this string begins with the data in @a s.
    @param s string data to compare to
    @param len length of @a s

    If @a len @< 0, then treats @a s as a null-terminated C string.

    @sa String::compare(const String &a, const String &b) */
bool
String::starts_with(const char *s, int len) const
{
    // See note on equals() re: "out-of-memory" strings.
    if (len < 0)
	len = strlen(s);
    return length() >= len && (data() == s || memcmp(data(), s, len) == 0);
}

/** @brief Compare this string with the data in @a s.
    @param s string data to compare to
    @param len length of @a s

    Same as String::compare(*this, String(s, len)).  If @a len @< 0, then
    treats @a s as a null-terminated C string.

    @sa String::compare(const String &a, const String &b) */
int
String::compare(const char *s, int len) const
{
    if (len < 0)
	len = strlen(s);
    int lencmp = length() - len, cmp;
    if (unlikely(data() == s))
	cmp = 0;
    else
	cmp = memcmp(data(), s, lencmp < 0 ? length() : len);
    return cmp ? cmp : lencmp;
}

/** @brief Return a pointer to the next character in UTF-8 encoding.
    @pre @a first @< @a last

    If @a first doesn't point at a valid UTF-8 character, returns @a first. */
const unsigned char *
String::skip_utf8_char(const unsigned char *first, const unsigned char *last)
{
    int c = *first;
    if (c > 0 && c < 0x80)
        return first + 1;
    else if (c < 0xC2)
	/* zero, or bad/overlong encoding */;
    else if (c < 0xE0) {	// 2 bytes: U+80-U+7FF
        if (likely(first + 1 < last
                   && first[1] >= 0x80 && first[1] < 0xC0))
            return first + 2;
    } else if (c < 0xF0) {	// 3 bytes: U+800-U+FFFF
        if (likely(first + 2 < last
                   && first[1] >= 0x80 && first[1] < 0xC0
		   && first[2] >= 0x80 && first[2] < 0xC0
                   && (c != 0xE0 || first[1] >= 0xA0) /* not overlong encoding */
                   && (c != 0xED || first[1] < 0xA0) /* not surrogate */))
            return first + 3;
    } else if (c < 0xF5) {	// 4 bytes: U+10000-U+10FFFF
        if (likely(first + 3 < last
                   && first[1] >= 0x80 && first[1] < 0xC0
		   && first[2] >= 0x80 && first[2] < 0xC0
		   && first[3] >= 0x80 && first[3] < 0xC0
                   && (c != 0xF0 || first[1] >= 0x90) /* not overlong encoding */
                   && (c != 0xF4 || first[1] < 0x90) /* not >U+10FFFF */))
            return first + 4;
    }
    return first;
}

CLICK_ENDDECLS
