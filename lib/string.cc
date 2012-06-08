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
 * "out-of-memory" string is returned instead.  Out-of-memory strings are
 * contagious: the result of any concatenation operation involving an
 * out-of-memory string is another out-of-memory string.  Thus, the final
 * result of a series of String operations will be an out-of-memory string,
 * even if the out-of-memory condition occurs in the middle.
 *
 * Out-of-memory strings have zero characters, but they aren't equal to other
 * empty strings.  If @a s is a normal String (even an empty string), and @a
 * oom is an out-of-memory string, then @a s @< @a oom.
 *
 * All out-of-memory strings are equal and share the same data(), which is
 * different from the data() of any other string.  See
 * String::out_of_memory_data().  The String::make_out_of_memory() function
 * returns an out-of-memory string.
 */

const char String::null_data = '\0';
const char String::oom_data = '\0';
const char String::bool_data[] = "true\0false";
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
    &oom_data, 0, 0
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
String::String(double x)
{
    char buf[128];
    int len = sprintf(buf, "%.12g", x);
    assign(buf, len, false);
}
#endif

String
String::make_claim(char *str, int len, int capacity)
{
    assert(str && len > 0 && capacity >= len);
    memo_t *new_memo = create_memo(str - MEMO_SPACE, len, capacity);
    return String(str, len, new_memo);
}

String
String::make_stable(const char *s, int len)
{
    if (len < 0)
	len = (s ? strlen(s) : 0);
    return String(s, len, 0);
}

String
String::make_uninitialized(int len)
{
    String s;
    s.append_uninitialized(len);
    return s;
}

String
String::make_numeric(int_large_t num, int base, bool uppercase)
{
    StringAccum sa;
    sa.append_numeric(num, base, uppercase);
    return sa.take_string();
}

String
String::make_numeric(uint_large_t num, int base, bool uppercase)
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
    _r.memo = 0;
    _r.data = &oom_data;
    _r.length = 0;
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
	_r.data = (str == &oom_data ? str : &null_data);

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

char *
String::append_uninitialized(int len)
{
    // Appending anything to "out of memory" leaves it as "out of memory"
    if (len <= 0 || _r.data == &oom_data)
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

    if (s == &oom_data)
	// Appending "out of memory" to a regular string makes it "out of
	// memory"
	assign_out_of_memory();
    else if (unlikely(len == 0))
	/* do nothing */;
    else if (unlikely(_r.length == 0 && memo && !out_of_memory())) {
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

void
String::append_fill(int c, int len)
{
    assert(len >= 0);
    if (char *space = append_uninitialized(len))
	memset(space, c, len);
}

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
    String do_not_delete_underlying_memo(*this);
    deref();
    assign(_r.data, _r.length, false);
    return const_cast<char *>(_r.data);
}

char *
String::mutable_c_str()
{
  (void) mutable_data();
  (void) c_str();
  return const_cast<char *>(_r.data);
}

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

int
String::find_left(char c, int start) const
{
    if (start < 0)
	start = 0;
    for (int i = start; i < _r.length; i++)
	if (_r.data[i] == c)
	    return i;
    return -1;
}

int
String::find_left(const String &str, int start) const
{
    if (start < 0)
	start = 0;
    if (start >= length())
	return -1;
    if (!str.length())
	return 0;
    int first_c = (unsigned char)str[0];
    int pos = start, max_pos = length() - str.length();
    for (pos = find_left(first_c, pos); pos >= 0 && pos <= max_pos;
	 pos = find_left(first_c, pos + 1))
	if (!memcmp(_r.data + pos, str._r.data, str.length()))
	    return pos;
    return -1;
}

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

String
String::lower() const
{
    // avoid copies
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

String
String::printable() const
{
    // avoid copies
    for (int i = 0; i < _r.length; i++)
	if (_r.data[i] < 32 || _r.data[i] > 126)
	    return hard_printable(*this, i);
    return *this;
}

String
String::trim_space() const
{
    for (int i = _r.length - 1; i >= 0; i--)
	if (!isspace((unsigned char) _r.data[i]))
	    return substring(0, i + 1);
    // return out-of-memory string if input is out-of-memory string
    return (_r.length ? String() : *this);
}

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

uint32_t
String::hashcode(const char *begin, const char *end)
{
    if (end <= begin)
	return 0;

    uint32_t hash = end - begin;
    int rem = hash & 3;
    end -= rem;
    uint32_t last16;

#if !HAVE_INDIFFERENT_ALIGNMENT
    if (!(reinterpret_cast<uintptr_t>(begin) & 1)) {
#endif
#define get16(p) (*reinterpret_cast<const uint16_t *>((p)))
	for (; begin != end; begin += 4) {
	    hash += get16(begin);
	    uint32_t tmp = (get16(begin + 2) << 11) ^ hash;
	    hash = (hash << 16) ^ tmp;
	    hash += hash >> 11;
	}
	if (rem >= 2) {
	    last16 = get16(begin);
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
	for (; begin != end; begin += 4) {
	    hash += get16(begin);
	    uint32_t tmp = (get16(begin + 2) << 11) ^ hash;
	    hash = (hash << 16) ^ tmp;
	    hash += hash >> 11;
	}
	if (rem >= 2) {
	    last16 = get16(begin);
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
	    hash ^= ((unsigned char) begin[2]) << 18;
	    hash += hash >> 11;
	} else {
	    hash += last16;
	    hash ^= hash << 11;
	    hash += hash >> 17;
	}
    } else if (rem == 1) {
	hash += (unsigned char) *begin;
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

#if 0
// 11.Apr.2008 -- This old hash function was swapped out in favor of
// SuperFastHash, above.
size_t
String::hashcode() const
{
    int l = length();
    const char *d = data();
    if (!l)
	return 0;
    else if (l == 1)
	return d[0] | (d[0] << 8);
    else if (l < 4)
	return d[0] + (d[1] << 3) + (l << 12);
    else
	return d[0] + (d[1] << 8) + (d[2] << 16) + (d[3] << 24)
	    + (l << 12) + (d[l-1] << 10);
}
#endif

bool
String::equals(const char *s, int len) const
{
    // It'd be nice to make "out-of-memory" strings compare unequal to
    // anything, even themselves, but this would be a bad idea for Strings
    // used as (for example) keys in hashtables. Instead, "out-of-memory"
    // strings compare unequal to other null strings, but equal to each other.
    if (len < 0)
	len = strlen(s);
    if (_r.length != len)
	return false;
    else if (_r.data == s)
	return true;
    else if (len == 0)
	return (s != &oom_data && _r.data != &oom_data);
    else
	return memcmp(_r.data, s, len) == 0;
}

bool
String::starts_with(const char *s, int len) const
{
    // See note on equals() re: "out-of-memory" strings.
    if (len < 0)
	len = strlen(s);
    if (_r.length < len)
	return false;
    else if (_r.data == s)
	return true;
    else if (len == 0)
	return (s != &oom_data && _r.data != &oom_data);
    else
	return memcmp(_r.data, s, len) == 0;
}

int
String::compare(const char *s, int len) const
{
    if (len < 0)
	len = strlen(s);
    if (_r.data == s)
	return _r.length - len;
    else if (_r.data == &oom_data)
	return 1;
    else if (s == &oom_data)
	return -1;
    else if (_r.length == len)
	return memcmp(_r.data, s, len);
    else if (_r.length < len) {
	int v = memcmp(_r.data, s, _r.length);
	return (v ? v : -1);
    } else {
	int v = memcmp(_r.data, s, len);
	return (v ? v : 1);
    }
}

CLICK_ENDDECLS
