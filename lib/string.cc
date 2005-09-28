// -*- c-basic-offset: 4; related-file-name: "../include/click/string.hh" -*-
/*
 * string.{cc,hh} -- a String class with shared substrings
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2004-2005 Regents of the University of California
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
 * <h3>Initialization</h3>
 *
 * The String implementation must be explicitly initialized before use; see
 * static_initialize().  Explicit initialization is used because static
 * constructors and other automatic initialization tricks don't work in the
 * kernel.  However, at user level, you can declare a String::Initializer
 * object to initialize the library.
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
 * String::out_of_memory_data().  The String::out_of_memory_string() function
 * returns an out-of-memory string.
 */

String::Memo *String::null_memo = 0;
String::Memo *String::permanent_memo = 0;
String::Memo *String::oom_memo = 0;
String *String::null_string_p = 0;
String *String::oom_string_p = 0;
const char String::oom_string_data = 0;

/** @cond never */
inline
String::Memo::Memo()
  : _refcount(0), _capacity(0), _dirty(0), _real_data("")
{
}

inline
String::Memo::Memo(char *data, int dirty, int capacity)
  : _refcount(0), _capacity(capacity), _dirty(dirty),
    _real_data(data)
{
}

String::Memo::Memo(int dirty, int capacity)
  : _refcount(1), _capacity(capacity), _dirty(dirty),
    _real_data(new char[capacity])
{
  assert(_capacity >= _dirty);
}

String::Memo::~Memo()
{
  if (_capacity) {
    assert(_capacity >= _dirty);
    delete[] _real_data;
  }
}
/** @endcond never */


/** @brief Create a String containing the ASCII base-10 representation of @a i.
 */
String::String(int i)
{
  char buf[128];
  sprintf(buf, "%d", i);
  assign(buf, -1);
}

/** @brief Create a String containing the ASCII base-10 representation of @a u.
 */
String::String(unsigned u)
{
  char buf[128];
  sprintf(buf, "%u", u);
  assign(buf, -1);
}

/** @brief Create a String containing the ASCII base-10 representation of @a i.
 */
String::String(long i)
{
  char buf[128];
  sprintf(buf, "%ld", i);
  assign(buf, -1);
}

/** @brief Create a String containing the ASCII base-10 representation of @a u.
 */
String::String(unsigned long u)
{
  char buf[128];
  sprintf(buf, "%lu", u);
  assign(buf, -1);
}

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG
// Implemented a lovely [u]int64_t converter in StringAccum
// (use the code even at user level to hunt out bugs)

/** @brief Create a String containing the ASCII base-10 representation of @a q.
 */
String::String(int64_t q)
{
  StringAccum sa;
  sa << q;
  assign(sa.data(), sa.length());
}

/** @brief Create a String containing the ASCII base-10 representation of @a q.
 */
String::String(uint64_t q)
{
  StringAccum sa;
  sa << q;
  assign(sa.data(), sa.length());
}

#endif

#ifdef CLICK_USERLEVEL

/** @brief Create a String containing the ASCII base-10 representation of @a d.
 * @note This function is only available at user level.
 */
String::String(double d)
{
  char buf[128];
  int len = sprintf(buf, "%.12g", d);
  assign(buf, len);
}

#endif

String
String::claim_string(char *str, int len, int capacity)
{
  assert(str && len > 0 && capacity >= len);
  Memo *new_memo = new Memo(str, len, capacity);
  if (new_memo)
    return String(str, len, new_memo);
  else
    return String(&oom_string_data, 0, oom_memo);
}

/** @brief Return a String that directly references the first @a len
 * characters of @a s.
 *
 * This function is suitable for static constant strings whose data is known
 * to stay around forever, such as C string constants.  If @a len @< 0, treats
 * @a s as a null-terminated C string.
 */
String
String::stable_string(const char *s, int len)
{
  if (len < 0)
    len = (s ? strlen(s) : 0);
  return String(s, len, permanent_memo);
}

/** @brief Create and return a String containing @a len random characters. */
String
String::garbage_string(int len)
{
  String s;
  s.append_garbage(len);
  return s;
}

void
String::make_out_of_memory()
{
  if (_memo)
    deref();
  _memo = oom_memo;
  _memo->_refcount++;
  _data = _memo->_real_data;
  _length = 0;
}

void
String::assign(const char *str, int len)
{
  if (!str) {
    assert(len <= 0);
    len = 0;
  } else if (len < 0)
    len = strlen(str);
  
  if (len == 0) {
    _memo = (str == &oom_string_data ? oom_memo : null_memo);
    _memo->_refcount++;
    
  } else {
    // Make 'capacity' a multiple of 16 characters and bigger than 'len'.
    int capacity = (len + 16) & ~15;
    _memo = new Memo(len, capacity);
    if (!_memo || !_memo->_real_data) {
      make_out_of_memory();
      return;
    }
    memcpy(_memo->_real_data, str, len);
  }
  
  _data = _memo->_real_data;
  _length = len;
}

/** @brief Append @a len random characters to this string. */
char *
String::append_garbage(int len)
{
    // Appending anything to "out of memory" leaves it as "out of memory"
    if (len <= 0 || _memo == oom_memo)
	return 0;
  
    // If we can, append into unused space. First, we check that there's
    // enough unused space for `len' characters to fit; then, we check
    // that the unused space immediately follows the data in `*this'.
    if (_memo->_capacity > _memo->_dirty + len) {
	char *real_dirty = _memo->_real_data + _memo->_dirty;
	if (real_dirty == _data + _length) {
	    _length += len;
	    _memo->_dirty += len;
	    assert(_memo->_dirty < _memo->_capacity);
	    return real_dirty;
	}
    }
  
    // Now we have to make new space. Make sure the new capacity is a
    // multiple of 16 characters and that it is at least 16.
    int new_capacity = (_length + 16) & ~15;
    while (new_capacity < _length + len)
	new_capacity *= 2;
    Memo *new_memo = new Memo(_length + len, new_capacity);
    if (!new_memo || !new_memo->_real_data) {
	delete new_memo;
	make_out_of_memory();
	return 0;
    }

    char *new_data = new_memo->_real_data;
    memcpy(new_data, _data, _length);
  
    deref();
    _data = new_data;
    new_data += _length;	// now new_data points to the garbage
    _length += len;
    _memo = new_memo;
    return new_data;
}

/** @brief Append the first @a len characters of @a suffix to this string.
 *
 * @param suffix data to append
 * @param len length of data
 *
 * If @a len @< 0, treats @a suffix as a null-terminated C string. */ 
void
String::append(const char *suffix, int len)
{
    if (!suffix) {
	assert(len <= 0);
	len = 0;
    } else if (len < 0)
	len = strlen(suffix);

    if (suffix == &oom_string_data)
	// Appending "out of memory" to a regular string makes it "out of
	// memory"
	make_out_of_memory();
    else if (char *space = append_garbage(len))
	memcpy(space, suffix, len);
}

/** @brief Append @a len copies of the character @a c to this string. */
void
String::append_fill(int c, int len)
{
    assert(len >= 0);
    if (char *space = append_garbage(len))
	memset(space, c, len);
}

/** @brief Ensure the string's data is unshared and return a mutable pointer
 * to it. */
char *
String::mutable_data()
{
  // If _memo has a capacity (it's not one of the special strings) and it's
  // uniquely referenced, return _data right away.
  if (_memo->_capacity && _memo->_refcount == 1)
    return const_cast<char *>(_data);
  
  // Otherwise, make a copy of it. Rely on: deref() doesn't change _data or
  // _length; and if _capacity == 0, then deref() doesn't free _real_data.
  assert(!_memo->_capacity || _memo->_refcount > 1);
  deref();
  assign(_data, _length);
  return const_cast<char *>(_data);
}

/** @brief Null-terminates the string and returns a mutable pointer to its
 * data.
 * @sa String::c_str */
char *
String::mutable_c_str()
{
  (void) mutable_data();
  (void) c_str();
  return const_cast<char *>(_data);
}

/** @brief Null-terminates the string.
 *
 * The terminating null character isn't considered part of the string, so
 * this->length() doesn't change.  Returns a corresponding C string pointer.
 * The returned pointer is semi-temporary; it will persist until the string is
 * destroyed, or someone appends to it. */
const char *
String::c_str() const
{
  // If _memo has no capacity, then this is one of the special strings (null
  // or PermString). We are guaranteed, in these strings, that _data[_length]
  // exists. We can return _data immediately if we have a '\0' in the right
  // place.
  if (!_memo->_capacity && _data[_length] == '\0')
    return _data;
  
  // Otherwise, this invariant must hold (there's more real data in _memo than
  // in our substring).
  assert(!_memo->_capacity
	 || _memo->_real_data + _memo->_dirty >= _data + _length);
  
  // Has the character after our substring been set?
  if (_memo->_real_data + _memo->_dirty == _data + _length) {
    // Character after our substring has not been set. May be able to change
    // it to '\0'. This case will never occur on special strings.
    if (_memo->_dirty < _memo->_capacity)
      goto add_final_nul;
    
  } else {
    // Character after our substring has been set. OK to return _data if it is
    // already '\0'.
    if (_data[_length] == '\0')
      return _data;
  }
  
  // If we get here, we must make a copy of our portion of the string.
  {
    String s(_data, _length);
    deref();
    assign(s);
  }
  
 add_final_nul:
  char *real_data = const_cast<char *>(_data);
  real_data[_length] = '\0';
  _memo->_dirty++;		// include '\0' in used portion of _memo
  return _data;
}

/** @brief Returns a substring of this string, consisting of the @a len
 * characters starting at index @a pos.
 * 
 * @param pos substring's first position relative to the string.
 * @param len length of the substring.
 *
 * If @a pos is negative, starts that far from the end of the string.  If @a
 * len is negative, leaves that many characters off the end of the string.
 * (This follows perl's semantics.)  Returns a null string if the adjusted @a
 * pos is out of range.  Truncates the substring if @a len goes beyond the end
 * of the string.
 */
String
String::substring(int pos, int len) const
{
    if (pos < 0)
	pos += _length;
    if (len < 0)
	len = _length - pos + len;
    if (pos + len > _length)
	len = _length - pos;
  
    if (pos < 0 || len <= 0)
	return String();
    else
	return String(_data + pos, len, _memo);
}

/** @brief Search for a character in a string.
 *
 * @param c character to search for
 * @param start initial search position
 *
 * Return the index of the leftmost occurence of @a c, starting at index @a
 * start and working up to the end of the string.  Returns -1 if @a c is not
 * found. */
int
String::find_left(char c, int start) const
{
    if (start < 0)
	start = 0;
    for (int i = start; i < _length; i++)
	if (_data[i] == c)
	    return i;
    return -1;
}

/** @brief Search for a substring in a string.
 *
 * @param str substring to search for
 * @param start initial search position
 *
 * Return the index of the leftmost occurence of the substring @a str, starting
 * at index @a start and working up to the end of the string.  Returns -1 if
 * @a str is not found. */
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
	if (!memcmp(_data + pos, str._data, str.length()))
	    return pos;
    return -1;
}

/** @brief Search for a character in a string.
 *
 * @param c character to search for
 * @param start initial search position
 *
 * Return the index of the rightmost occurence of the character @a c, starting
 * at index @a start and working back to the beginning of the string.  Returns
 * -1 if @a c is not found.  @a start may start beyond the end of the
 * string. */
int
String::find_right(char c, int start) const
{
    if (start >= _length)
	start = _length - 1;
    for (int i = start; i >= 0; i--)
	if (_data[i] == c)
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
	x[pos] = tolower(x[pos]);
    return new_s;
}

/** @brief Returns a lowercased version of this string.
 *
 * Translates the ASCII characters 'A' through 'Z' into their lowercase
 * equivalents. */
String
String::lower() const
{
    // avoid copies
    for (int i = 0; i < _length; i++)
	if (_data[i] >= 'A' && _data[i] <= 'Z')
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
	x[pos] = toupper(x[pos]);
    return new_s;
}

/** @brief Returns an uppercased version of this string.
 *
 * Translates the ASCII characters 'a' through 'z' into their uppercase
 * equivalents. */
String
String::upper() const
{
    // avoid copies
    for (int i = 0; i < _length; i++)
	if (_data[i] >= 'a' && _data[i] <= 'z')
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

/** @brief Returns a "printable" version of this string.
 *
 * Translates control characters 0-31 into "control" sequences, such as "^@"
 * for the null character, and characters 127-255 into octal escape sequences,
 * such as "\377" for 255. */
String
String::printable() const
{
    // avoid copies
    for (int i = 0; i < _length; i++)
	if (_data[i] < 32 || _data[i] > 126)
	    return hard_printable(*this, i);
    return *this;
}

/** @brief Returns a substring with spaces trimmed from the end. */
String
String::trim_space() const
{
    for (int i = _length - 1; i >= 0; i--)
	if (!isspace(_data[i]))
	    return substring(0, i + 1);
    // return out-of-memory string if input is out-of-memory string
    return (_length ? String() : *this);
}

/** @brief Returns a hex-quoted version of the string.
 *
 * For example, the string "Abcd" would convert to "\<41626364>". */
String
String::quoted_hex() const
{
    static const char hex_digits[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    StringAccum sa;
    char *buf;
    if (out_of_memory() || !(buf = sa.extend(length() * 2 + 3)))
	return out_of_memory_string();
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

int
hashcode(const String &s)
{
    int length = s.length();
    const char *data = s.data();
    if (!length)
	return 0;
    else if (length == 1)
	return data[0] | (data[0] << 8);
    else if (length < 4)
	return data[0] + (data[1] << 3) + (length << 12);
    else
	return data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24)
	    + (length << 12) + (data[length-1] << 10);
}

/** @brief Return true iff this string is equal to the data in @a s.
 * @param s string data to compare to
 * @param len length of @a s
 * 
 * Same as String::compare(*this, String(s, len)) == 0.  If @a len @< 0, then
 * treats @a s as a null-terminated C string.
 *
 * @sa String::compare(const String &a, const String &b) */
bool
String::equals(const char *s, int len) const
{
    // It'd be nice to make "out-of-memory" strings compare unequal to
    // anything, even themseleves, but this would be a bad idea for Strings
    // used as (for example) keys in hashtables. Instead, "out-of-memory"
    // strings compare unequal to other null strings, but equal to each other.
    if (len < 0)
	len = strlen(s);
    if (_length != len)
	return false;
    else if (_data == s)
	return true;
    else if (len == 0)
	return (s != &oom_string_data && _memo != oom_memo);
    else
	return memcmp(_data, s, len) == 0;
}

/** @brief Compare this string with the data in @a s.
 * @param s string data to compare to
 * @param len length of @a s
 * 
 * Same as String::compare(*this, String(s, len)).  If @a len @< 0, then treats
 * @a s as a null-terminated C string.
 *
 * @sa String::compare(const String &a, const String &b) */
int
String::compare(const char *s, int len) const
{
    if (len < 0)
	len = strlen(s);
    if (_data == s)
	return _length - len;
    else if (_memo == oom_memo)
	return 1;
    else if (s == &oom_string_data)
	return -1;
    else if (_length == len)
	return memcmp(_data, s, len);
    else if (_length < len) {
	int v = memcmp(_data, s, _length);
	return (v ? v : -1);
    } else {
	int v = memcmp(_data, s, len);
	return (v ? v : 1);
    }
}


/** @class String::Initializer
 *
 * This class's constructor initializes the String implementation by calling
 * String::static_initialize().  You should declare a String::Initializer
 * object at global scope in any file that declares a global String object.
 * For example:
 * @code
 *    static String::Initializer initializer;
 *    String global_string = "100";
 * @endcode */

String::Initializer::Initializer()
{
    String::static_initialize();
}

/** @brief Initialize the String implementation.
 *
 * This function must be called before any String functionality is used.  It
 * is safe to call it multiple times.
 *
 * @note Elements don't need to worry about static_initialize(); Click drivers
 * have already called it for you.
 *
 * @sa String::Initializer */
void
String::static_initialize()
{
    // function called to initialize static globals
    if (!null_memo) {
#if CLICK_DMALLOC
	CLICK_DMALLOC_REG("str0");
#endif
	null_memo = new Memo;
	null_memo->_refcount++;
	permanent_memo = new Memo;
	permanent_memo->_refcount++;
	// use a separate string for oom_memo's data, so we can distinguish
	// the pointer
	oom_memo = new Memo;
	oom_memo->_refcount++;
	oom_memo->_real_data = const_cast<char*>(&oom_string_data);
	null_string_p = new String;
	oom_string_p = new String(&oom_string_data, 0, oom_memo);
#if CLICK_DMALLOC
	CLICK_DMALLOC_REG("????");
#endif
    }
}

/** @brief Clean up the String implementation.
 *
 * Call this function to release any memory allocated by the String
 * implementation. */
void
String::static_cleanup()
{
    if (null_string_p) {
	delete null_string_p;
	null_string_p = 0;
	delete oom_string_p;
	oom_string_p = 0;
	if (--oom_memo->_refcount == 0)
	    delete oom_memo;
	if (--permanent_memo->_refcount == 0)
	    delete permanent_memo;
	if (--null_memo->_refcount == 0)
	    delete null_memo;
	null_memo = permanent_memo = oom_memo = 0;
    }
}

CLICK_ENDDECLS
