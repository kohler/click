// -*- c-basic-offset: 4; related-file-name: "../../lib/straccum.cc" -*-
#ifndef CLICK_STRACCUM_HH
#define CLICK_STRACCUM_HH
#include <click/glue.hh>
#include <click/string.hh>
#ifdef CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <asm/string.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#elif defined(CLICK_BSDMODULE)
# include <sys/systm.h>
#else	/* User-space */
# include <string.h>
#endif
#if __GNUC__ > 4
# define CLICK_SNPRINTF_ATTR __attribute__((__format__(__printf__, 3, 4)))
#else
# define CLICK_SNPRINTF_ATTR /* nothing */
#endif
CLICK_DECLS

/** @file <click/straccum.hh>
    @brief Click's StringAccum class, used to construct Strings efficiently from pieces.
*/

class StringAccum { public:

    typedef const char *const_iterator;
    typedef char *iterator;

    typedef int (StringAccum::*unspecified_bool_type)() const;

    inline StringAccum();
    explicit inline StringAccum(int capacity);
    inline StringAccum(const String &str);
    inline StringAccum(const StringAccum &x);
#if HAVE_CXX_RVALUE_REFERENCES
    inline StringAccum(StringAccum &&x);
#endif
    inline ~StringAccum();

    inline StringAccum &operator=(const StringAccum &x);
#if HAVE_CXX_RVALUE_REFERENCES
    inline StringAccum &operator=(StringAccum &&x);
#endif

    inline const char *data() const;
    inline char *data();
    inline int length() const;
    inline int capacity() const;

    const char *c_str();

    inline operator unspecified_bool_type() const;
    inline bool empty() const;
    inline bool operator!() const;

    inline const_iterator begin() const;
    inline iterator begin();
    inline const_iterator end() const;
    inline iterator end();

    inline char operator[](int i) const;
    inline char &operator[](int i);
    inline char front() const;
    inline char &front();
    inline char back() const;
    inline char &back();

    inline bool out_of_memory() const;
    void assign_out_of_memory();

    inline void clear();
    inline char *reserve(int n);
    inline void set_length(int len);
    int resize(int len);
    inline void adjust_length(int delta);
    inline char *extend(int nadjust, int nreserve = 0);

    inline void pop_back(int n = 1);

    inline void append(char c);
    inline void append(unsigned char c);
    inline bool append_utf8(int ch);
    inline void append(const char *cstr);
    inline void append(const char *s, int len);
    inline void append(const unsigned char *s, int len);
    inline void append(const char *first, const char *last);
    inline void append(const unsigned char *first, const unsigned char *last);
    void append_fill(int c, int len);
    void append_numeric(String::int_large_t x, int base = 10, bool uppercase = true);
    void append_numeric(String::uint_large_t x, int base = 10, bool uppercase = true);

    StringAccum &snprintf(int n, const char *format, ...) CLICK_SNPRINTF_ATTR;

    String take_string();

    void swap(StringAccum &x);

    // see also operator<< declarations below

    inline void forward(int delta) CLICK_DEPRECATED;

  private:

    enum {
	MEMO_SPACE = String::MEMO_SPACE
    };

    struct rep_t {
	unsigned char *s;
	int len;
	int cap;
	rep_t()
	    : s(reinterpret_cast<unsigned char *>(const_cast<char *>(String::empty_data()))),
	      len(0), cap(0) {
	}
	explicit rep_t(uninitialized_type) {
	}
    };

    rep_t r_;

    char *grow(int);
    char *hard_extend(int nadjust, int nreserve);
    void hard_append(const char *s, int len);
    void hard_append_cstr(const char *cstr);
    bool append_utf8_hard(int ch);

    friend StringAccum &operator<<(StringAccum &sa, const String &str);
#if HAVE_PERMSTRING
    friend StringAccum &operator<<(StringAccum &sa, PermString s);
#endif

};

inline StringAccum &operator<<(StringAccum &sa, char c);
inline StringAccum &operator<<(StringAccum &sa, unsigned char c);
inline StringAccum &operator<<(StringAccum &sa, const char *cstr);
inline StringAccum &operator<<(StringAccum &sa, const String &s);
inline StringAccum &operator<<(StringAccum &sa, const StringAccum &x);

inline StringAccum &operator<<(StringAccum &sa, bool x);
inline StringAccum &operator<<(StringAccum &sa, short x);
inline StringAccum &operator<<(StringAccum &sa, unsigned short x);
inline StringAccum &operator<<(StringAccum &sa, int x);
inline StringAccum &operator<<(StringAccum &sa, unsigned x);
StringAccum &operator<<(StringAccum &sa, long x);
StringAccum &operator<<(StringAccum &sa, unsigned long x);
#if HAVE_LONG_LONG
inline StringAccum &operator<<(StringAccum &sa, long long x);
inline StringAccum &operator<<(StringAccum &sa, unsigned long long x);
#endif
#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
inline StringAccum &operator<<(StringAccum &sa, int64_t x);
inline StringAccum &operator<<(StringAccum &sa, uint64_t x);
#endif
#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
StringAccum &operator<<(StringAccum &sa, double x);
#endif
StringAccum &operator<<(StringAccum &sa, void *ptr);


/** @brief Construct an empty StringAccum (with length 0). */
inline StringAccum::StringAccum() {
}

/** @brief Construct a StringAccum with room for at least @a capacity
    characters.
    @param capacity initial capacity

    If @a capacity == 0, the StringAccum is created empty. If @a capacity is
    too large (so that @a capacity bytes of memory can't be allocated), the
    StringAccum falls back to a smaller capacity (possibly zero). */
inline StringAccum::StringAccum(int capacity) {
    assert(capacity >= 0);
    unsigned char *s;
    if (capacity
	&& (s = (unsigned char *) CLICK_LALLOC(capacity + MEMO_SPACE))) {
	r_.s = s + MEMO_SPACE;
	r_.cap = capacity;
    }
}

/** @brief Construct a StringAccum containing the characters in @a str. */
inline StringAccum::StringAccum(const String &str) {
    append(str.begin(), str.end());
}

/** @brief Construct a StringAccum containing a copy of @a x. */
inline StringAccum::StringAccum(const StringAccum &x) {
    append(x.data(), x.length());
}

#if HAVE_CXX_RVALUE_REFERENCES
/** @brief Move-construct a StringAccum from @a x. */
inline StringAccum::StringAccum(StringAccum &&x)
    : r_(x.r_) {
    x.r_.cap = 0;
}
#endif

/** @brief Destroy a StringAccum, freeing its memory. */
inline StringAccum::~StringAccum() {
    if (r_.cap > 0)
	CLICK_LFREE(r_.s - MEMO_SPACE, r_.cap + MEMO_SPACE);
}

/** @brief Return the contents of the StringAccum.
    @return The StringAccum's contents.

    The returned data() value points to length() bytes of writable memory
    (unless the StringAccum itself is const). */
inline const char *StringAccum::data() const {
    return reinterpret_cast<const char *>(r_.s);
}

/** @overload */
inline char *StringAccum::data() {
    return reinterpret_cast<char *>(r_.s);
}

/** @brief Return the length of the StringAccum. */
inline int StringAccum::length() const {
    return r_.len;
}

/** @brief Return the StringAccum's current capacity.

    The capacity is the maximum length the StringAccum can hold without
    incurring a memory allocation. Returns -1 for out-of-memory
    StringAccums. */
inline int StringAccum::capacity() const {
    return r_.cap;
}

/** @brief Return an iterator for the first character in the StringAccum.

    StringAccum iterators are simply pointers into string data, so they are
    quite efficient. @sa StringAccum::data */
inline StringAccum::const_iterator StringAccum::begin() const {
    return reinterpret_cast<char *>(r_.s);
}

/** @overload */
inline StringAccum::iterator StringAccum::begin() {
    return reinterpret_cast<char *>(r_.s);
}

/** @brief Return an iterator for the end of the StringAccum.

    The return value points one character beyond the last character in the
    StringAccum. */
inline StringAccum::const_iterator StringAccum::end() const {
    return reinterpret_cast<char *>(r_.s + r_.len);
}

/** @overload */
inline StringAccum::iterator StringAccum::end() {
    return reinterpret_cast<char *>(r_.s + r_.len);
}

/** @brief Test if the StringAccum contains characters. */
inline StringAccum::operator unspecified_bool_type() const {
    return r_.len != 0 ? &StringAccum::capacity : 0;
}

/** @brief Test if the StringAccum is empty.

    Returns true iff length() == 0. */
inline bool StringAccum::operator!() const {
    return r_.len == 0;
}

/** @brief Test if the StringAccum is empty. */
inline bool StringAccum::empty() const {
    return r_.len == 0;
}

/** @brief Test if the StringAccum is out-of-memory. */
inline bool StringAccum::out_of_memory() const {
    return unlikely(r_.cap < 0);
}

/** @brief Return the <a>i</a>th character in the string.
    @param i character index
    @pre 0 <= @a i < length() */
inline char StringAccum::operator[](int i) const {
    assert((unsigned) i < (unsigned) r_.len);
    return static_cast<char>(r_.s[i]);
}

/** @brief Return a reference to the <a>i</a>th character in the string.
    @param i character index
    @pre 0 <= @a i < length() */
inline char &StringAccum::operator[](int i) {
    assert((unsigned) i < (unsigned) r_.len);
    return reinterpret_cast<char &>(r_.s[i]);
}

/** @brief Return the first character in the string.
    @pre length() > 0 */
inline char StringAccum::front() const {
    assert(r_.len > 0);
    return static_cast<char>(r_.s[0]);
}

/** @brief Return a reference to the first character in the string.
    @pre length() > 0 */
inline char &StringAccum::front() {
    assert(r_.len > 0);
    return reinterpret_cast<char &>(r_.s[0]);
}

/** @brief Return the last character in the string.
    @pre length() > 0 */
inline char StringAccum::back() const {
    assert(r_.len > 0);
    return static_cast<char>(r_.s[r_.len - 1]);
}

/** @brief Return a reference to the last character in the string.
    @pre length() > 0 */
inline char &StringAccum::back() {
    assert(r_.len > 0);
    return reinterpret_cast<char &>(r_.s[r_.len - 1]);
}

/** @brief Clear the StringAccum's comments.

    All characters in the StringAccum are erased. Also resets the
    StringAccum's out-of-memory status. */
inline void StringAccum::clear() {
    if (r_.cap < 0)
	r_.cap = 0;
    r_.len = 0;
}

/** @brief Reserve space for at least @a n characters.
    @return a pointer to at least @a n characters, or null if allocation
    fails
    @pre @a n >= 0

    reserve() does not change the string's length(), only its capacity(). In
    a frequent usage pattern, code calls reserve(), passing an upper bound
    on the characters that could be written by a series of operations. After
    writing into the returned buffer, adjust_length() is called to account
    for the number of characters actually written. */
inline char *StringAccum::reserve(int n) {
    assert(n >= 0);
    if (r_.len + n <= r_.cap)
	return reinterpret_cast<char *>(r_.s + r_.len);
    else
	return grow(r_.len + n);
}

/** @brief Set the StringAccum's length to @a len.
    @param len new length in characters
    @pre 0 <= @a len <= capacity()
    @sa adjust_length */
inline void StringAccum::set_length(int len) {
    assert(len >= 0 && r_.len <= r_.cap);
    r_.len = len;
}

/** @brief Adjust the StringAccum's length.
    @param delta  length adjustment
    @pre If @a delta > 0, then length() + @a delta <= capacity().
    If @a delta < 0, then length() + delta >= 0.

    The StringAccum's length after adjust_length(@a delta) equals its old
    length plus @a delta. Generally adjust_length() is used after a call to
    reserve(). @sa set_length */
inline void StringAccum::adjust_length(int delta) {
    assert(r_.len + delta >= 0 && r_.len + delta <= r_.cap);
    r_.len += delta;
}

/** @brief Reserve space and adjust length in one operation.
    @param nadjust number of characters to reserve and adjust length
    @param nreserve additional characters to reserve
    @pre @a nadjust >= 0 and @a nreserve >= 0

    This operation combines the effects of reserve(@a nadjust + @a nreserve)
    and adjust_length(@a nadjust). Returns the result of the reserve()
    call. */
inline char *StringAccum::extend(int nadjust, int nreserve) {
#if CLICK_OPTIMIZE_SIZE || __OPTIMIZE_SIZE__
    return hard_extend(nadjust, nreserve);
#else
    assert(nadjust >= 0 && nreserve >= 0);
    if (r_.len + nadjust + nreserve <= r_.cap) {
	char *x = reinterpret_cast<char *>(r_.s + r_.len);
	r_.len += nadjust;
	return x;
    } else
	return hard_extend(nadjust, nreserve);
#endif
}

/** @brief Remove characters from the end of the StringAccum.
    @param n number of characters to remove
    @pre @a n >= 0 and @a n <= length()

    Same as adjust_length(-@a n). */
inline void StringAccum::pop_back(int n) {
    assert(n >= 0 && r_.len >= n);
    r_.len -= n;
}

/** @brief Append character @a c to the StringAccum.
    @param c character to append */
inline void StringAccum::append(char c) {
    if (r_.len < r_.cap || grow(r_.len))
	r_.s[r_.len++] = c;
}

/** @overload */
inline void StringAccum::append(unsigned char c) {
    append(static_cast<char>(c));
}

/** @brief Append the first @a len characters of @a s to this StringAccum.
    @param s data to append
    @param len length of data
    @pre @a len >= 0 */
inline void StringAccum::append(const char *s, int len) {
#if CLICK_OPTIMIZE_SIZE || __OPTIMIZE_SIZE__
    hard_append(s, len);
#else
    assert(len >= 0);
    if (r_.len + len <= r_.cap) {
	memcpy(r_.s + r_.len, s, len);
	r_.len += len;
    } else
	hard_append(s, len);
#endif
}

/** @brief Append the null-terminated C string @a s to this StringAccum.
    @param s data to append */
inline void StringAccum::append(const char *cstr) {
    if (__builtin_constant_p(strlen(cstr)))
	append(cstr, strlen(cstr));
    else
	hard_append_cstr(cstr);
}

/** @overload */
inline void StringAccum::append(const unsigned char *s, int len) {
    append(reinterpret_cast<const char *>(s), len);
}

/** @brief Append the data from @a first to @a last to the end of this
    StringAccum.

    Does nothing if @a first >= @a last. */
inline void StringAccum::append(const char *first, const char *last) {
    if (first < last)
	append(first, last - first);
}

/** @overload */
inline void StringAccum::append(const unsigned char *first, const unsigned char *last) {
    if (first < last)
	append(first, last - first);
}

/** @brief Append Unicode character @a ch encoded in UTF-8.
    @return true if character was valid.

    Appends nothing if @a ch is not a valid Unicode character. */
inline bool StringAccum::append_utf8(int ch) {
    if (unlikely(ch <= 0))
	return false;
    else if (likely(ch <= 0x7F)) {
	append(static_cast<char>(ch));
	return true;
    } else
	return append_utf8_hard(ch);
}

/** @brief Assign this StringAccum to @a x. */
inline StringAccum &StringAccum::operator=(const StringAccum &x) {
    if (&x != this) {
	if (out_of_memory())
	    r_.cap = 0;
	r_.len = 0;
	append(x.data(), x.length());
    }
    return *this;
}

#if HAVE_CXX_RVALUE_REFERENCES
/** @brief Move-assign this StringAccum to @a x. */
inline StringAccum &StringAccum::operator=(StringAccum &&x) {
    x.swap(*this);
    return *this;
}
#endif

/** @cond never */
/** @brief Adjust the StringAccum's length (deprecated).
    @param delta length adjustment
    @deprecated Use adjust_length() instead. */
inline void StringAccum::forward(int delta) {
    adjust_length(delta);
}
/** @endcond never */

/** @relates StringAccum
    @brief Append character @a c to StringAccum @a sa.
    @return @a sa
    @note Same as @a sa.append(@a c). */
inline StringAccum &operator<<(StringAccum &sa, char c) {
    sa.append(c);
    return sa;
}

/** @relates StringAccum
    @brief Append character @a c to StringAccum @a sa.
    @return @a sa
    @note Same as @a sa.append(@a c). */
inline StringAccum &operator<<(StringAccum &sa, unsigned char c) {
    sa.append(c);
    return sa;
}

/** @relates StringAccum
    @brief Append null-terminated C string @a cstr to StringAccum @a sa.
    @return @a sa
    @note Same as @a sa.append(@a cstr). */
inline StringAccum &operator<<(StringAccum &sa, const char *cstr) {
    sa.append(cstr);
    return sa;
}

/** @relates StringAccum
    @brief Append "true" or "false" to @a sa, depending on @a x.
    @return @a sa */
inline StringAccum &operator<<(StringAccum &sa, bool x) {
    sa.append(String::bool_data + (-x & 6), 5 - x);
    return sa;
}

/** @relates StringAccum
    @brief Append decimal representation of @a x to @a sa.
    @return @a sa */
inline StringAccum &operator<<(StringAccum &sa, short x) {
    return sa << static_cast<long>(x);
}

/** @overload */
inline StringAccum &operator<<(StringAccum &sa, unsigned short x) {
    return sa << static_cast<unsigned long>(x);
}

/** @overload */
inline StringAccum &operator<<(StringAccum &sa, int x) {
    return sa << static_cast<long>(x);
}

/** @overload */
inline StringAccum &operator<<(StringAccum &sa, unsigned x) {
    return sa << static_cast<unsigned long>(x);
}

#if HAVE_LONG_LONG
/** @overload */
inline StringAccum &operator<<(StringAccum &sa, long long x) {
    sa.append_numeric(static_cast<String::int_large_t>(x));
    return sa;
}

/** @overload */
inline StringAccum &operator<<(StringAccum &sa, unsigned long long x) {
    sa.append_numeric(static_cast<String::uint_large_t>(x));
    return sa;
}
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
/** @overload */
inline StringAccum &operator<<(StringAccum &sa, int64_t x) {
    sa.append_numeric(static_cast<String::int_large_t>(x));
    return sa;
}

/** @overload */
inline StringAccum &operator<<(StringAccum &sa, uint64_t x) {
    sa.append_numeric(static_cast<String::uint_large_t>(x));
    return sa;
}
#endif

/** @relates StringAccum
    @brief Append the contents of @a str to @a sa.
    @return @a sa */
inline StringAccum &operator<<(StringAccum &sa, const String &str) {
    sa.append(str.data(), str.length());
    return sa;
}

/** @relates StringAccum
    @brief Append the contents of @a x to @a sa.
    @return @a sa */
inline StringAccum &operator<<(StringAccum &sa, const StringAccum &x) {
    sa.append(x.begin(), x.end());
    return sa;
}

inline void click_swap(StringAccum &a, StringAccum &b) {
    a.swap(b);
}

inline void assign_consume(StringAccum &a, StringAccum &b) {
    a.swap(b);
}

#undef CLICK_SNPRINTF_ATTR
CLICK_ENDDECLS
#endif
