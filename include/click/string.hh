// -*- related-file-name: "../../lib/string.cc" -*-
#ifndef CLICK_STRING_HH
#define CLICK_STRING_HH
#include <click/algorithm.hh>
#include <click/atomic.hh>
#if HAVE_STRING_PROFILING
# include <click/integers.hh>
#endif
#if CLICK_LINUXMODULE || CLICK_BSDMODULE
# include <click/glue.hh>
#else
# include <string.h>
#endif
#define CLICK_CONSTANT_CSTR(cstr) ((cstr) && __builtin_constant_p(strlen((cstr))))
CLICK_DECLS
class StringAccum;

class String { public:

    typedef const char *const_iterator;
    typedef const_iterator iterator;

    typedef int (String::*unspecified_bool_type)() const;

#if HAVE_INT64_TYPES && (!HAVE_LONG_LONG || SIZEOF_LONG_LONG <= 8)
    typedef int64_t intmax_t;
    typedef uint64_t uintmax_t;
#elif HAVE_LONG_LONG
    typedef long long intmax_t;
    typedef unsigned long long uintmax_t;
#else
    typedef long intmax_t;
    typedef unsigned long uintmax_t;
#endif
    typedef intmax_t int_large_t;
    typedef uintmax_t uint_large_t;

    inline String();
    inline String(const String &x);
#if HAVE_CXX_RVALUE_REFERENCES
    inline String(String &&x);
#endif
    inline String(const char *cstr);
    inline String(const char *s, int len);
    inline String(const unsigned char *s, int len);
    inline String(const char *first, const char *last);
    inline String(const unsigned char *first, const unsigned char *last);
    explicit inline String(bool x);
    explicit inline String(char c);
    explicit inline String(unsigned char c);
    explicit String(int x);
    explicit String(unsigned x);
    explicit String(long x);
    explicit String(unsigned long x);
#if HAVE_LONG_LONG
    explicit String(long long x);
    explicit String(unsigned long long x);
#endif
#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
    explicit String(int64_t x);
    explicit String(uint64_t x);
#endif
#if HAVE_FLOAT_TYPES
    explicit String(double x);
#endif
    inline ~String();

    static inline const String &make_empty();
    static inline String make_uninitialized(int len);
    static inline String make_garbage(int len) CLICK_DEPRECATED;
    static inline String make_stable(const char *cstr);
    static inline String make_stable(const char *s, int len);
    static inline String make_stable(const char *first, const char *last);
    static String make_numeric(intmax_t x, int base = 10, bool uppercase = true);
    static String make_numeric(uintmax_t x, int base = 10, bool uppercase = true);

    inline const char *data() const;
    inline int length() const;

    inline const char *c_str() const;

    inline operator unspecified_bool_type() const;
    inline bool empty() const;
    inline bool operator!() const;

    inline const_iterator begin() const;
    inline const_iterator end() const;

    inline char operator[](int i) const;
    inline char at(int i) const;
    inline char front() const;
    inline char back() const;

    static uint32_t hashcode(const char *begin, const char *end);
    static inline uint32_t hashcode(const unsigned char *begin,
				    const unsigned char *end);
    inline uint32_t hashcode() const;

    inline String substring(const char *begin, const char *end) const;
    String substring(int pos, int len) const;
    inline String substring(int pos) const;
    String trim_space() const;

    inline bool equals(const String &x) const;
    inline bool equals(const char *s, int len) const;
    static inline int compare(const String &a, const String &b);
    inline int compare(const String &x) const;
    int compare(const char *s, int len) const;
    inline bool starts_with(const String &x) const;
    bool starts_with(const char *s, int len) const;

    // bool operator==(const String &, const String &);
    // bool operator==(const String &, const char *);
    // bool operator==(const char *, const String &);
    // bool operator!=(const String &, const String &);
    // bool operator!=(const String &, const char *);
    // bool operator!=(const char *, const String &);
    // bool operator<(const String &, const String &);
    // bool operator<=(const String &, const String &);
    // bool operator>(const String &, const String &);
    // bool operator>=(const String &, const String &);

    int find_left(char c, int start = 0) const;
    int find_left(const String &x, int start = 0) const;
    int find_right(char c, int start = 0x7FFFFFFF) const;

    String lower() const;
    String upper() const;
    String printable() const;
    String quoted_hex() const;
    String encode_json() const;

    inline String &operator=(const String &x);
#if HAVE_CXX_RVALUE_REFERENCES
    inline String &operator=(String &&x);
#endif
    inline String &operator=(const char *cstr);

    inline void swap(String &x);

    inline void append(const String &x);
    inline void append(const char *cstr);
    inline void append(const char *s, int len);
    inline void append(const char *first, const char *last);
    inline void append(char c);
    void append_fill(int c, int len);
    char *append_uninitialized(int len);
    inline char *append_garbage(int len) CLICK_DEPRECATED;

    inline String &operator+=(const String &x);
    inline String &operator+=(const char *cstr);
    inline String &operator+=(char c);

    // String operator+(String, const String &);
    // String operator+(String, const char *);
    // String operator+(const char *, const String &);

    inline String compact() const;

    inline bool data_shared() const;
    char *mutable_data();
    char *mutable_c_str();

    static inline const String &make_out_of_memory();
    inline bool out_of_memory() const;
    static inline const char *out_of_memory_data();
    static inline int out_of_memory_length();
    static inline const char *empty_data();

#if HAVE_STRING_PROFILING
    static void profile_report(StringAccum &sa, int examples = 0);
#endif

    static inline const char *skip_utf8_char(const char *first, const char *last);
    static const unsigned char *skip_utf8_char(const unsigned char *first,
					       const unsigned char *last);

    static const char bool_data[11];

  private:

    /** @cond never */
    struct memo_t {
	volatile uint32_t refcount;
	uint32_t capacity;
	volatile uint32_t dirty;
#if HAVE_STRING_PROFILING > 1
	memo_t **pprev;
	memo_t *next;
#endif
	char real_data[8];	// but it might be more or less
    };

    enum {
	MEMO_SPACE = sizeof(memo_t) - 8
    };

    struct rep_t {
	const char *data;
	int length;
	memo_t *memo;
    };
    /** @endcond never */

    mutable rep_t _r;		// mutable for c_str()

#if HAVE_STRING_PROFILING
    static uint64_t live_memo_count;
    static uint64_t memo_sizes[55];
    static uint64_t live_memo_sizes[55];
    static uint64_t live_memo_bytes[55];
# if HAVE_STRING_PROFILING > 1
    static memo_t *live_memos[55];
# endif

    static inline int profile_memo_size_bucket(uint32_t dirty, uint32_t capacity) {
	if (capacity <= 16)
	    return dirty;
	else if (capacity <= 32)
	    return 17 + (capacity - 17) / 2;
	else if (capacity <= 64)
	    return 25 + (capacity - 33) / 8;
	else
	    return 29 + 26 - ffs_msb(capacity - 1);
    }

    static void profile_update_memo_dirty(memo_t *memo, uint32_t old_dirty, uint32_t new_dirty, uint32_t capacity) {
	if (capacity <= 16 && new_dirty != old_dirty) {
	    ++memo_sizes[new_dirty];
	    ++live_memo_sizes[new_dirty];
	    live_memo_bytes[new_dirty] += capacity;
	    --live_memo_sizes[old_dirty];
	    live_memo_bytes[old_dirty] -= capacity;
# if HAVE_STRING_PROFILING > 1
	    if ((*memo->pprev = memo->next))
		memo->next->pprev = memo->pprev;
	    memo->pprev = &live_memos[new_dirty];
	    if ((memo->next = *memo->pprev))
		memo->next->pprev = &memo->next;
	    *memo->pprev = memo;
# else
	    (void) memo;
# endif
	}
    }

    static void one_profile_report(StringAccum &sa, int i, int examples);
#endif

    inline void assign_memo(const char *data, int length, memo_t *memo) const {
	_r.data = data;
	_r.length = length;
	if ((_r.memo = memo))
	    atomic_uint32_t::inc(memo->refcount);
    }

    inline String(const char *data, int length, memo_t *memo) {
	assign_memo(data, length, memo);
    }

    inline void assign(const String &x) const {
	assign_memo(x._r.data, x._r.length, x._r.memo);
    }

    inline void deref() const {
	if (_r.memo && atomic_uint32_t::dec_and_test(_r.memo->refcount))
	    delete_memo(_r.memo);
    }

    void assign(const char *s, int len, bool need_deref);
    void assign_out_of_memory();
    void append(const char *s, int len, memo_t *memo);
    static String hard_make_stable(const char *s, int len);
    static inline memo_t *absent_memo() {
	return reinterpret_cast<memo_t *>(uintptr_t(1));
    }
    static memo_t *create_memo(char *space, int dirty, int capacity);
    static void delete_memo(memo_t *memo);
    const char *hard_c_str() const;
    bool hard_equals(const char *s, int len) const;

    static const char null_data;
    static const char oom_data[15];
    static const char int_data[20];
    static const rep_t null_string_rep;
    static const rep_t oom_string_rep;
    enum { oom_len = 14 };

    static String make_claim(char *, int, int); // claim memory

    friend struct rep_t;
    friend class StringAccum;

};

class StringRef {
  public:

    inline StringRef();
    inline StringRef(const StringRef &x);
    inline StringRef(const char *cstr);
    inline StringRef(const char *s, int len);
    inline StringRef(const String &x);

    inline const char *data() const;
    inline int length() const;

    inline const char *begin() const;
    inline const char *end() const;

    inline uint32_t hashcode() const;

  private:
    const char *data_;
    int len_;
};

/** @brief Construct an empty String (with length 0). */
inline String::String() {
    assign_memo(&null_data, 0, 0);
}

/** @brief Construct a copy of the String @a x. */
inline String::String(const String &x) {
    assign(x);
}

#if HAVE_CXX_RVALUE_REFERENCES
/** @brief Move-construct a String from @a x. */
inline String::String(String &&x)
    : _r(x._r) {
    x._r.memo = 0;
}
#endif

/** @brief Construct a String containing the C string @a cstr.
    @param cstr a null-terminated C string
    @return A String containing the characters of @a cstr, up to but not
    including the terminating null character. */
inline String::String(const char *cstr) {
    if (CLICK_CONSTANT_CSTR(cstr))
	assign_memo(cstr, strlen(cstr), 0);
    else
	assign(cstr, -1, false);
}

/** @brief Construct a String containing the first @a len characters of
    string @a s.
    @param s a string
    @param len number of characters to take from @a s.  If @a len @< 0,
    then takes @c strlen(@a s) characters.
    @return A String containing @a len characters of @a s. */
inline String::String(const char *s, int len) {
    assign(s, len, false);
}

/** @overload */
inline String::String(const unsigned char *s, int len) {
    assign(reinterpret_cast<const char *>(s), len, false);
}

/** @brief Construct a String containing the characters from @a first
    to @a last.
    @param first first character in string (begin iterator)
    @param last pointer one past last character in string (end iterator)
    @return A String containing the characters from @a first to @a last.

    Constructs an empty string if @a first @>= @a last. */
inline String::String(const char *first, const char *last) {
    assign(first, (first < last ? last - first : 0), false);
}

/** @overload */
inline String::String(const unsigned char *first, const unsigned char *last) {
    assign(reinterpret_cast<const char *>(first),
	   (first < last ? last - first : 0), false);
}

/** @brief Construct a String equal to "true" or "false" depending on the
    value of @a x. */
inline String::String(bool x) {
    // bool_data equals "false\0true\0"
    assign_memo(bool_data + (-x & 6), 5 - x, 0);
}

/** @brief Construct a String containing the single character @a c. */
inline String::String(char c) {
    assign(&c, 1, false);
}

/** @overload */
inline String::String(unsigned char c) {
    assign(reinterpret_cast<char *>(&c), 1, false);
}

/** @brief Destroy a String, freeing memory if necessary. */
inline String::~String() {
    deref();
}

/** @brief Return a const reference to an empty String.

    May be quicker than String::String(). */
inline const String &String::make_empty() {
    return reinterpret_cast<const String &>(null_string_rep);
}

/** @brief Return a String containing @a len unknown characters. */
inline String String::make_uninitialized(int len) {
    String s;
    s.append_uninitialized(len);
    return s;
}

/** @cond never */
inline String String::make_garbage(int len) {
    return make_uninitialized(len);
}
/** @endcond never */

/** @brief Return a String that directly references the C string @a cstr.

    The make_stable() functions are suitable for static constant strings
    whose data is known to stay around forever, such as C string constants.

    @warning The String implementation may access @a cstr's terminating null
    character. */
inline String String::make_stable(const char *cstr) {
    if (CLICK_CONSTANT_CSTR(cstr))
	return String(cstr, strlen(cstr), 0);
    else
	return hard_make_stable(cstr, -1);
}

/** @brief Return a String that directly references the first @a len
    characters of @a s.

    If @a len @< 0, treats @a s as a null-terminated C string.

    @warning The String implementation may access @a s[@a len], which
    should remain constant even though it's not part of the String. */
inline String String::make_stable(const char *s, int len) {
    if (__builtin_constant_p(len) && len >= 0)
	return String(s, len, 0);
    else
	return hard_make_stable(s, len);
}

/** @brief Return a String that directly references the character data in
    [@a first, @a last).
    @param first pointer to the first character in the character data
    @param last pointer one beyond the last character in the character data
    (but see the warning)

    This function is suitable for static constant strings whose data is
    known to stay around forever, such as C string constants.  Returns an
    empty string if @a first @>= @a last.

    @warning The String implementation may access *@a last, which should
    remain constant even though it's not part of the String. */
inline String String::make_stable(const char *first, const char *last) {
    return String(first, (first < last ? last - first : 0), 0);
}

/** @brief Return a pointer to the string's data.

    Only the first length() characters are valid, and the string
    might not be null-terminated. */
inline const char *String::data() const {
    return _r.data;
}

/** @brief Return the string's length. */
inline int String::length() const {
    return _r.length;
}

/** @brief Null-terminate the string.

    The terminating null character isn't considered part of the string, so
    this->length() doesn't change.  Returns a corresponding C string
    pointer.  The returned pointer is semi-temporary; it will persist until
    the string is destroyed or appended to. */
inline const char *String::c_str() const {
    // See also hard_c_str().
#if CLICK_OPTIMIZE_SIZE || __OPTIMIZE_SIZE__
    return hard_c_str();
#else
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
#endif
}

/** @brief Return a substring of the current string starting at @a first
    and ending before @a last.
    @param first pointer to the first substring character
    @param last pointer one beyond the last substring character

    Returns an empty string if @a first @>= @a last. Also returns an empty
    string if @a first or @a last is out of range (i.e., either less than
    this->begin() or greater than this->end()), but this should be
    considered a programming error; a future version may generate a warning
    for this case. */
inline String String::substring(const char *first, const char *last) const {
    if (first < last && first >= _r.data && last <= _r.data + _r.length)
	return String(first, last - first, _r.memo);
    else
	return String();
}

/** @brief Return the suffix of the current string starting at index @a pos.

    If @a pos is negative, starts that far from the end of the string.
    If @a pos is so negative that the suffix starts outside the string,
    then the entire string is returned. If the substring is beyond the
    end of the string (@a pos > length()), returns an empty string (but
    this should be considered a programming error; a future version may
    generate a warning for this case).

    @note String::substring() is intended to behave like Perl's
    substr(). */
inline String String::substring(int pos) const {
    return substring((pos <= -_r.length ? 0 : pos), _r.length);
}

/** @brief Return an iterator for the first character in the string.

    String iterators are simply pointers into string data, so they are
    quite efficient.  @sa String::data */
inline String::const_iterator String::begin() const {
    return _r.data;
}

/** @brief Return an iterator for the end of the string.

    The return value points one character beyond the last character in the
    string. */
inline String::const_iterator String::end() const {
    return _r.data + _r.length;
}

/** @brief Test if the string is nonempty. */
inline String::operator unspecified_bool_type() const {
    return (_r.length != 0 ? &String::length : 0);
}

/** @brief Test if the string is empty. */
inline bool String::empty() const {
    return _r.length == 0;
}

/** @brief Test if the string is empty. */
inline bool String::operator!() const {
    return empty();
}

/** @brief Return the @a i th character in the string.

    Does not check bounds.  @sa String::at */
inline char String::operator[](int i) const {
    return _r.data[i];
}

/** @brief Return the @a i th character in the string.

    Checks bounds: an assertion will fail if @a i is less than 0 or not less
    than length(). @sa String::operator[] */
inline char String::at(int i) const {
    assert((unsigned) i < (unsigned) _r.length);
    return _r.data[i];
}

/** @brief Return the first character in the string.

    Does not check bounds.  Same as (*this)[0]. */
inline char String::front() const {
    return _r.data[0];
}

/** @brief Return the last character in the string.

    Does not check bounds.  Same as (*this)[length() - 1]. */
inline char String::back() const {
    return _r.data[_r.length - 1];
}

/** @overload */
inline uint32_t String::hashcode(const unsigned char *first,
				 const unsigned char *last) {
    return hashcode(reinterpret_cast<const char *>(first),
		    reinterpret_cast<const char *>(last));
}

/** @brief Returns a 32-bit hash function of this string's characters.

    Equivalent to String::hashcode(begin(), end()).  Uses Paul Hsieh's
    "SuperFastHash."

    @invariant  If s1 == s2, then s1.hashcode() == s2.hashcode(). */
inline uint32_t String::hashcode() const {
    return (length() ? hashcode(begin(), end()) : 0);
}

/** @brief Test if this string equals @a x. */
inline bool String::equals(const String &x) const {
    return equals(x.data(), x.length());
}

/** @brief Test if this string is equal to the data in @a s.
    @param s string data to compare to
    @param len length of @a s

    Same as String::compare(*this, String(s, len)) == 0. If @a len @< 0,
    then treats @a s as a null-terminated C string.

    @sa String::compare(const String &a, const String &b) */
inline bool String::equals(const char *s, int len) const {
#if CLICK_OPTIMIZE_SIZE || __OPTIMIZE_SIZE__
    return hard_equals(s, len);
#else
    if (__builtin_constant_p(len) && len >= 0)
	return length() == len && memcmp(data(), s, len) == 0;
    else
	return hard_equals(s, len);
#endif
}

/** @brief Compare two strings.
    @param a first string to compare
    @param b second string to compare

    Returns 0 if @a a == @a b, negative if @a a @< @a b in lexicographic
    order, and positive if @a a @> @a b in lexicographic order. The
    lexicographic order treats all characters as unsigned. */
inline int String::compare(const String &a, const String &b) {
    return a.compare(b);
}

/** @brief Compare this string with string @a x.

    Same as String::compare(*this, @a x).
    @sa String::compare(const String &a, const String &b) */
inline int String::compare(const String &x) const {
    return compare(x.data(), x.length());
}

/** @brief Test if this string begins with prefix @a x.

    Same as String::starts_with(@a x.data(), @a x.length()). */
inline bool String::starts_with(const String &x) const {
    return starts_with(x.data(), x.length());
}

/** @brief Assign this string to @a x. */
inline String &String::operator=(const String &x) {
    if (likely(&x != this)) {
	deref();
	assign(x);
    }
    return *this;
}

#if HAVE_CXX_RVALUE_REFERENCES
/** @brief Move-assign this string to @a x. */
inline String &String::operator=(String &&x) {
    deref();
    _r = x._r;
    x._r.memo = 0;
    return *this;
}
#endif

/** @brief Assign this string to the C string @a cstr. */
inline String &String::operator=(const char *cstr) {
    if (CLICK_CONSTANT_CSTR(cstr)) {
	deref();
	assign_memo(cstr, strlen(cstr), 0);
    } else
	assign(cstr, -1, true);
    return *this;
}

/** @brief Swap the values of this string and @a x. */
inline void String::swap(String &x) {
    rep_t r = _r;
    _r = x._r;
    x._r = r;
}

/** @brief Append @a x to this string. */
inline void String::append(const String &x) {
    append(x.data(), x.length(), x._r.memo);
}

/** @brief Append the null-terminated C string @a cstr to this string.
    @param cstr data to append */
inline void String::append(const char *cstr) {
    if (CLICK_CONSTANT_CSTR(cstr))
	append(cstr, strlen(cstr), absent_memo());
    else
	append(cstr, -1, absent_memo());
}

/** @brief Append the first @a len characters of @a s to this string.
    @param s data to append
    @param len length of data
    @pre @a len @>= 0 */
inline void String::append(const char *s, int len) {
    append(s, len, absent_memo());
}

/** @brief Appends the data from @a first to @a last to this string.

    Does nothing if @a first @>= @a last. */
inline void String::append(const char *first, const char *last) {
    if (first < last)
	append(first, last - first);
}

/** @brief Append the character @a c to this string. */
inline void String::append(char c) {
    append(&c, 1, absent_memo());
}

/** @cond never */
inline char *String::append_garbage(int len) {
    return append_uninitialized(len);
}
/** @endcond never */

/** @brief Append @a x to this string.
    @return *this */
inline String &String::operator+=(const String &x) {
    append(x.data(), x.length(), x._r.memo);
    return *this;
}

/** @brief Append the null-terminated C string @a cstr to this string.
    @return *this */
inline String &String::operator+=(const char *cstr) {
    append(cstr);
    return *this;
}

/** @brief Append the character @a c to this string.
    @return *this */
inline String &String::operator+=(char c) {
    append(&c, 1);
    return *this;
}

/** @brief Test if the String's data is shared or immutable. */
inline bool String::data_shared() const {
    return !_r.memo || _r.memo->refcount != 1;
}

/** @brief Return a compact version of this String.

    The compact version shares no more than 256 bytes of data with any other
    non-stable String. */
inline String String::compact() const {
    if (!_r.memo || _r.memo->refcount == 1
	|| (uint32_t) _r.length + 256 >= _r.memo->capacity)
	return *this;
    else
	return String(_r.data, _r.data + _r.length);
}

/** @brief Test if this is an out-of-memory string. */
inline bool String::out_of_memory() const {
    return unlikely(data() == oom_data);
}

/** @brief Return a const reference to a canonical out-of-memory String. */
inline const String &String::make_out_of_memory() {
    return reinterpret_cast<const String &>(oom_string_rep);
}

/** @brief Return the data pointer used for out-of-memory strings. */
inline const char *String::out_of_memory_data() {
    return oom_data;
}

/** @brief Return the length of canonical out-of-memory strings. */
inline int String::out_of_memory_length() {
    return oom_len;
}

/** @brief Return the data pointer used for canonical empty strings.

    The returned value may be dereferenced; it points to a null
    character. */
inline const char *String::empty_data() {
    return &null_data;
}

/** @brief Return a pointer to the next character in UTF-8 encoding.
    @pre @a first @< @a last

    If @a first doesn't point at a valid UTF-8 character, returns @a first. */
inline const char *String::skip_utf8_char(const char *first, const char *last) {
    return reinterpret_cast<const char *>(skip_utf8_char(reinterpret_cast<const unsigned char *>(first), reinterpret_cast<const unsigned char *>(last)));
}

inline StringRef::StringRef()
    : data_(0), len_(0) {
}

inline StringRef::StringRef(const StringRef &x)
    : data_(x.data_), len_(x.len_) {
}

inline StringRef::StringRef(const char *cstr)
    : data_(cstr), len_(strlen(cstr)) {
}

inline StringRef::StringRef(const char *s, int len)
    : data_(s), len_(len) {
}

inline StringRef::StringRef(const String &x)
    : data_(x.data()), len_(x.length()) {
}

inline const char *StringRef::data() const {
    return data_;
}

inline int StringRef::length() const {
    return len_;
}

inline const char *StringRef::begin() const {
    return data();
}

inline const char *StringRef::end() const {
    return data() + length();
}

/** @relates String
    @brief Compares two strings for equality.

    Returns true iff the two operands have the same lengths and the same
    characters in the same order.  At most one of the operands can be a
    null-terminated C string.
    @sa String::compare */
inline bool operator==(const String &a, const String &b) {
    return a.equals(b.data(), b.length());
}

/** @relates String */
inline bool operator==(const char *a, const String &b) {
    if (CLICK_CONSTANT_CSTR(a))
	return b.equals(a, strlen(a));
    else
	return b.equals(a, -1);
}

/** @relates String */
inline bool operator==(const String &a, const char *b) {
    if (CLICK_CONSTANT_CSTR(b))
	return a.equals(b, strlen(b));
    else
	return a.equals(b, -1);
}

/** @relates String
    @brief Compare two Strings for inequality.

    Returns true iff !(@a a == @a b).  At most one of the operands can be a
    null-terminated C string. */
inline bool operator!=(const String &a, const String &b) {
    return !(a == b);
}

/** @relates String */
inline bool operator!=(const char *a, const String &b) {
    return !(a == b);
}

/** @relates String */
inline bool operator!=(const String &a, const char *b) {
    return !(a == b);
}

/** @relates String
    @brief Compare two Strings.

    Returns true iff @a a @< @a b in lexicographic order.
    @sa String::compare */
inline bool operator<(const String &a, const String &b) {
    return a.compare(b.data(), b.length()) < 0;
}

/** @relates String
    @brief Compare two Strings.

    Returns true iff @a a @<= @a b in lexicographic order.
    @sa String::compare */
inline bool operator<=(const String &a, const String &b) {
    return a.compare(b.data(), b.length()) <= 0;
}

/** @relates String
    @brief Compare two Strings.

    Returns true iff @a a @> @a b in lexicographic order.
    @sa String::compare */
inline bool operator>(const String &a, const String &b) {
    return a.compare(b.data(), b.length()) > 0;
}

/** @relates String
    @brief Compare two Strings.

    Returns true iff @a a @>= @a b in lexicographic order.
    @sa String::compare */
inline bool operator>=(const String &a, const String &b) {
    return a.compare(b.data(), b.length()) >= 0;
}

/** @relates String
    @brief Concatenate the operands and return the result.

    At most one of the two operands can be a null-terminated C string. */
inline String operator+(String a, const String &b) {
    a += b;
    return a;
}

/** @relates String */
inline String operator+(String a, const char *b) {
    a.append(b);
    return a;
}

/** @relates String */
inline String operator+(const char *a, const String &b) {
    String s1(a);
    s1 += b;
    return s1;
}

/** @relates String
    @brief Concatenate the operands and return the result.

    The second operand is a single character. */
inline String operator+(String a, char b) {
    a.append(&b, 1);
    return a;
}

// find methods

inline const char *rfind(const char *first, const char *last, char c) {
    for (const char *bb = last - 1; bb >= first; bb--)
	if (*bb == c)
	    return bb;
    return last;
}

inline const char *find(const String &s, char c) {
    return find(s.begin(), s.end(), c);
}

// sort methods

template <typename T> int click_compare(const void *, const void *);

template <> inline int click_compare<String>(const void *a, const void *b) {
    const String *ta = reinterpret_cast<const String *>(a);
    const String *tb = reinterpret_cast<const String *>(b);
    return String::compare(*ta, *tb);
}

CLICK_ENDDECLS
#endif
