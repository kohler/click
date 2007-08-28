// -*- c-basic-offset: 4; related-file-name: "../../lib/straccum.cc" -*-
#ifndef CLICK_STRACCUM_HH
#define CLICK_STRACCUM_HH
#include <click/glue.hh>
#include <click/string.hh>
#ifdef CLICK_LINUXMODULE
# include <asm/string.h>
#elif defined(CLICK_BSDMODULE)
# include <sys/systm.h>
#else	/* User-space */
# include <string.h>
#endif
CLICK_DECLS

/** @file <click/straccum.hh>
    @brief Click's StringAccum class, used to construct Strings efficiently from pieces.
*/

class StringAccum { public:

    /** @brief Create an empty StringAccum (with length 0). */
    StringAccum()
	: _s(0), _len(0), _cap(0) {
    }
    
    explicit inline StringAccum(int);

    /** @brief Destroy a StringAccum, freeing its memory. */
    ~StringAccum() {
	if (_cap >= 0)
	    CLICK_LFREE(_s, _cap);
    }

    /** @brief Return the contents of the StringAccum.
	@return The StringAccum's contents.

	The return value is null if the StringAccum is empty or out-of-memory.
	The returned data() value points to writable memory (unless the
	StringAccum itself is const). */
    inline const char *data() const {
	return reinterpret_cast<const char *>(_s);
    }

    /** @overload */
    inline char *data() {
	return reinterpret_cast<char *>(_s);
    }
    
    /** @brief Return the length of the StringAccum. */
    int length() const {
	return _len;
    }

    /** @brief Return the StringAccum's current capacity.
	
	The capacity is the maximum length the StringAccum can hold without
	incurring a memory allocation.  Returns -1 for out-of-memory
	StringAccums. */
    int capacity() const {
	return _cap;
    }
    
    typedef int StringAccum::*unspecified_bool_type;

    /** @brief Return true iff the StringAccum contains characters.

        Returns false for empty and out-of-memory StringAccums. */
    operator unspecified_bool_type() const {
	return _len != 0 ? &StringAccum::_len : 0;
    }

    /** @brief Returns true iff the StringAccum does not contain characters.

	Returns true for empty and out-of-memory StringAccums. */
    bool operator!() const {
	return _len == 0;
    }

    /** @brief Returns true iff the StringAccum is out-of-memory. */
    bool out_of_memory() const {
	return _cap < 0;
    }
  
    const char *c_str();

    /** @brief Returns the <a>i</a>th character in the string.
	@param  i  character index.

	@pre 0 <= @a i < length() */
    char operator[](int i) const {
	assert(i>=0 && i<_len);
	return static_cast<char>(_s[i]);
    }
    
    /** @brief Returns a reference to the <a>i</a>th character in the string.
	@param  i  character index.

	@pre 0 <= @a i < length() */
    char &operator[](int i) {
	assert(i>=0 && i<_len);
	return reinterpret_cast<char &>(_s[i]);
    }

    /** @brief Returns the last character in the string.
	@pre length() > 0 */
    char back() const {
	assert(_len > 0);
	return static_cast<char>(_s[_len - 1]);
    }

    /** @brief Returns a reference to the last character in the string.
	@pre length() > 0 */
    char &back() {
	assert(_len > 0);
	return reinterpret_cast<char &>(_s[_len - 1]);
    }

    inline void clear();
  
    inline char *reserve(int);
    void set_length(int len);
    void adjust_length(int n);
    inline char *extend(int nadjust, int nreserve = 0);
    void pop_back(int n = 1);
  
    inline void append(char);
    inline void append(unsigned char);
    inline void append(const char *s, int len);
    inline void append(const unsigned char *s, int len);
    inline void append(const char *begin, const char *end);
    void append_fill(int c, int len);

    void append_numeric(String::int_large_t num, int base = 10, bool uppercase = true);
    void append_numeric(String::uint_large_t num, int base = 10, bool uppercase = true);

    StringAccum &snprintf(int, const char *, ...);
  
    String take_string();

    void swap(StringAccum &);

    // see also operator<< declarations below
  
    void forward(int n) CLICK_DEPRECATED;

  private:
  
    unsigned char *_s;
    int _len;
    int _cap;
  
    void make_out_of_memory();
    inline void safe_append(const char *, int);
    bool grow(int);

    StringAccum(const StringAccum &);
    StringAccum &operator=(const StringAccum &);

    friend StringAccum &operator<<(StringAccum &, const char *);
  
};

inline StringAccum &operator<<(StringAccum &, char);
inline StringAccum &operator<<(StringAccum &, unsigned char);
inline StringAccum &operator<<(StringAccum &, const char *);
inline StringAccum &operator<<(StringAccum &, const String &);
inline StringAccum &operator<<(StringAccum &, const StringAccum &);
#ifdef HAVE_PERMSTRING
inline StringAccum &operator<<(StringAccum &, PermString);
#endif

inline StringAccum &operator<<(StringAccum &, bool);
inline StringAccum &operator<<(StringAccum &, short);
inline StringAccum &operator<<(StringAccum &, unsigned short);
inline StringAccum &operator<<(StringAccum &, int);
inline StringAccum &operator<<(StringAccum &, unsigned);
StringAccum &operator<<(StringAccum &, long);
StringAccum &operator<<(StringAccum &, unsigned long);
#if HAVE_LONG_LONG
inline StringAccum &operator<<(StringAccum &, long long);
inline StringAccum &operator<<(StringAccum &, unsigned long long);
#endif
#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
inline StringAccum &operator<<(StringAccum &, int64_t);
inline StringAccum &operator<<(StringAccum &, uint64_t);
#endif
#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
StringAccum &operator<<(StringAccum &, double);
#endif

StringAccum &operator<<(StringAccum &, void *);


/** @brief Create a StringAccum with room for at least @a capacity characters.
    @param  capacity  initial capacity.

    If @a capacity <= 0, the StringAccum is created empty.  If @a capacity is
    too large (so that @a capacity bytes of memory can't be allocated), the
    StringAccum is created as out-of-memory. */
inline
StringAccum::StringAccum(int capacity)
    : _len(0)
{
    assert(capacity >= 0);
    if (capacity) {
	_s = (unsigned char *) CLICK_LALLOC(capacity);
	_cap = (_s ? capacity : -1);
    } else {
	_s = 0;
	_cap = 0;
    }
}

/** @brief Reserve space for at least @a n characters.
    @param  n  number of characters to reserve.
    @return  a pointer to at least @a n characters, or null if allocation fails.
    @pre  @a n >= 0

    reserve() does not change the string's length(), only its capacity().  In
    a frequent usage pattern, code calls reserve(), passing an upper bound on
    the characters that could be written by a series of operations.  After
    writing into the returned buffer, adjust_length() is called to account for
    the number of characters actually written. */
inline char *
StringAccum::reserve(int n)
{
    assert(n >= 0);
    if (_len + n <= _cap || grow(_len + n))
	return (char *)(_s + _len);
    else
	return 0;
}

/** @brief Adjust the StringAccum's length.
    @param  n  length adjustment.
    @pre  If @a n > 0, then length() + @a n <= capacity().  If @a n < 0, then length() + n >= 0.

    Generally adjust_length() is used after a call to reserve().
    @sa set_length */
inline void
StringAccum::adjust_length(int n) {
    assert(_len + n >= 0 && _len + n <= _cap);
    _len += n;
}

/** @brief Adjust the StringAccum's length (deprecated).
    @param  n  length adjustment.
    @deprecated  Use adjust_length() instead. */
inline void
StringAccum::forward(int n)
{
    adjust_length(n);
}

/** @brief Reserve space and adjust length in one operation.
    @param  nadjust   number of characters to reserve and adjust length.
    @param  nreserve  additional characters to reserve.
    @pre  @a nadjust >= 0 and @a nreserve >= 0

    This operation combines the effects of reserve(@a nadjust + @a nreserve)
    and adjust_length(@a nadjust).  Returns the result of the reserve() call. */
inline char *
StringAccum::extend(int nadjust, int nreserve)
{
    assert(nadjust >= 0 && nreserve >= 0);
    char *c = reserve(nadjust + nreserve);
    if (c)
	_len += nadjust;
    return c;
}

/** @brief Remove characters from the end of the StringAccum.
    @param  n  number of characters to remove.
    @pre @a n >= 0

    Same as adjust_length(-@a n). */
inline void
StringAccum::pop_back(int n) {
    assert(n >= 0 && _len >= n);
    _len -= n;
}

/** @brief Sets the StringAccum's length to @a len.
    @param  len  new length in characters.
    @pre  0 <= @a len <= capacity()
    @sa adjust_length */
inline void
StringAccum::set_length(int len) {
    assert(len >= 0 && _len <= _cap);
    _len = len;
}

/** @brief Extend the StringAccum by character @a c.
    @param  c  character to extend */
inline void
StringAccum::append(unsigned char c)
{
    if (_len < _cap || grow(_len))
	_s[_len++] = c;
}

/** @overload */
inline void
StringAccum::append(char c)
{
    append(static_cast<unsigned char>(c));
}

inline void
StringAccum::safe_append(const char *s, int len)
{
    if (char *x = extend(len))
	memcpy(x, s, len);
}

/** @brief Append the first @a len characters of @a suffix to this StringAccum.
    @param  suffix  data to append
    @param  len     length of data

    If @a len < 0, treats @a suffix as a null-terminated C string. */
inline void
StringAccum::append(const char *suffix, int len)
{
    if (len < 0)
	len = strlen(suffix);
    if (len == 0 && suffix == String::out_of_memory_data())
	make_out_of_memory();
    safe_append(suffix, len);
}

/** @overload */
inline void
StringAccum::append(const unsigned char *suffix, int len)
{
    append(reinterpret_cast<const char *>(suffix), len);
}

/** @brief Append the data from @a begin to @a end to the end of this StringAccum.

    Does nothing if @a begin >= @a end. */
inline void
StringAccum::append(const char *begin, const char *end)
{
    if (begin < end)
	safe_append(begin, end - begin);
    else if (begin == String::out_of_memory_data())
	make_out_of_memory();
}

/** @brief Clear the StringAccum's comments.

    All characters in the StringAccum are erased.  This operation also resets
    the StringAccum's out-of-memory status. */
inline void
StringAccum::clear()
{
    if (_cap < 0)
	_cap = 0, _s = 0;
    _len = 0;
}

/** @relates StringAccum
    @brief Append character @a c to StringAccum @a sa.
    @return @a sa
    @note Same as @a sa.append(@a c). */
inline StringAccum &
operator<<(StringAccum &sa, char c)
{
    sa.append(c);
    return sa;
}

/** @relates StringAccum
    @brief Append character @a c to StringAccum @a sa.
    @return @a sa
    @note Same as @a sa.append(@a c). */
inline StringAccum &
operator<<(StringAccum &sa, unsigned char c)
{
    sa.append(c);
    return sa;
}

/** @relates StringAccum
    @brief Append null-terminated C string @a cstr to StringAccum @a sa.
    @return @a sa
    @note Same as @a sa.append(@a cstr, -1). */
inline StringAccum &
operator<<(StringAccum &sa, const char *cstr)
{
    sa.append(cstr, -1);
    return sa;
}

/** @relates StringAccum
    @brief Append "true" or "false" to @a sa, depending on @a b.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, bool b)
{
    return sa << (b ? "true" : "false");
}

/** @relates StringAccum
    @brief Append decimal representation of @a i to @a sa.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, short i)
{
    return sa << static_cast<long>(i);
}

/** @relates StringAccum
    @brief Append decimal representation of @a u to @a sa.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, unsigned short u)
{
    return sa << static_cast<unsigned long>(u);
}

/** @relates StringAccum
    @brief Append decimal representation of @a i to @a sa.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, int i)
{
    return sa << static_cast<long>(i);
}

/** @relates StringAccum
    @brief Append decimal representation of @a u to @a sa.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, unsigned u)
{
    return sa << static_cast<unsigned long>(u);
}

#if HAVE_LONG_LONG
/** @relates StringAccum
    @brief Append decimal representation of @a q to @a sa.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, long long q)
{
    sa.append_numeric(static_cast<String::int_large_t>(q));
    return sa;
}

/** @relates StringAccum
    @brief Append decimal representation of @a q to @a sa.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, unsigned long long q)
{
    sa.append_numeric(static_cast<String::uint_large_t>(q));
    return sa;
}
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
/** @relates StringAccum
    @brief Append decimal representation of @a q to @a sa.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, int64_t q)
{
    sa.append_numeric(static_cast<String::int_large_t>(q));
    return sa;
}

/** @relates StringAccum
    @brief Append decimal representation of @a q to @a sa.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, uint64_t q)
{
    sa.append_numeric(static_cast<String::uint_large_t>(q));
    return sa;
}
#endif

/** @relates StringAccum
    @brief Append the contents of @a str to @a sa.
    @return @a sa */
StringAccum &
operator<<(StringAccum &sa, const String &str)
{
    sa.append(str.data(), str.length());
    return sa;
}

#ifdef HAVE_PERMSTRING
inline StringAccum &
operator<<(StringAccum &sa, PermString s)
{
    sa.safe_append(s.c_str(), s.length());
    return sa;
}
#endif

/** @relates StringAccum
    @brief Append the contents of @a sb to @a sa.
    @return @a sa */
inline StringAccum &
operator<<(StringAccum &sa, const StringAccum &sb)
{
    sa.append(sb.data(), sb.length());
    return sa;
}

CLICK_ENDDECLS
#endif
