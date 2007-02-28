// -*- c-basic-offset: 2; related-file-name: "../../lib/string.cc" -*-
#ifndef CLICK_STRING_HH
#define CLICK_STRING_HH
#ifdef HAVE_PERMSTRING
# include "permstr.hh"
#endif
#include <click/algorithm.hh>
CLICK_DECLS

/** @file <click/string.hh>
 * @brief Click's String class.
 */

class String { public:
  
  // Call static_initialize() before any function which might deal with
  // Strings, and declare a String::Initializer in any file in which you
  // declare static global Strings.
  static void static_initialize();
  static void static_cleanup();
  struct Initializer { Initializer(); };

  inline String();
  inline String(const String &str);
  inline String(const char *cstr);
  inline String(const char *s, int len);
  inline String(const char *begin, const char *end);
  explicit inline String(bool b);
  explicit inline String(char c);
  explicit inline String(unsigned char c);
  explicit String(int i);
  explicit String(unsigned u);
  explicit String(long i);
  explicit String(unsigned long u);
#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG
  explicit String(int64_t q);
  explicit String(uint64_t q);
#endif
#ifdef CLICK_USERLEVEL
  explicit String(double d);
#endif
  inline ~String();
  
  static inline const String &empty_string();
  static String garbage_string(int len);	// len garbage characters
  static String stable_string(const char *s, int len = -1); // stable read-only mem.
  static inline String stable_string(const char *begin, const char *end);
  
  inline int length() const;
  inline const char *data() const;
  
  typedef const char *const_iterator;
  typedef const_iterator iterator;
  inline const_iterator begin() const;
  inline const_iterator end() const;
  
  inline operator bool() const;
  inline operator bool();
  
  inline char operator[](int i) const;
  inline char at(int i) const;
  inline char front() const;
  inline char back() const;
  
  const char *c_str() const;		// pointer returned is semi-transient
  
  bool equals(const char *s, int len) const;
  // bool operator==(const String &, const String &);
  // bool operator==(const String &, const char *);
  // bool operator==(const char *, const String &);
  // bool operator!=(const String &, const String &);
  // bool operator!=(const String &, const char *);
  // bool operator!=(const char *, const String &);

  static inline int compare(const String &a, const String &b);
  inline int compare(const String &str) const;
  int compare(const char *s, int len) const;
  // bool operator<(const String &, const String &);
  // bool operator<=(const String &, const String &);
  // bool operator>(const String &, const String &);
  // bool operator>=(const String &, const String &);

  inline String substring(const char *begin, const char *end) const;
  String substring(int pos, int len) const;
  inline String substring(int pos) const;
  
  int find_left(char c, int start = 0) const;
  int find_left(const String &s, int start = 0) const;
  int find_right(char c, int start = 0x7FFFFFFF) const;
  
  String lower() const;			// lowercase
  String upper() const;			// uppercase
  String printable() const;		// quote non-ASCII characters
  String trim_space() const;		// trim space from right
  String quoted_hex() const;		// hex enclosed in '\<...>'
  
  inline String &operator=(const String &str);
  inline String &operator=(const char *cstr);

  void append(const char *s, int len);
  inline void append(const char *begin, const char *end);
  void append_fill(int c, int len);
  char *append_garbage(int len);
  inline String &operator+=(const String &str);
  inline String &operator+=(const char *cstr);
  inline String &operator+=(char c);

  // String operator+(String, const String &);
  // String operator+(String, const char *);
  // String operator+(const char *, const String &);
  // String operator+(String, PermString);
  // String operator+(PermString, const String &);
  // String operator+(PermString, const char *);
  // String operator+(const char *, PermString);
  // String operator+(PermString, PermString);
  // String operator+(String, char);

  inline bool data_shared() const;
  char *mutable_data();
  char *mutable_c_str();

  inline bool out_of_memory() const;
  static inline const String &out_of_memory_string();
  static inline const char *out_of_memory_data();
    
 private:

  /** @cond never */
  struct Memo {
    int _refcount;
    int _capacity;
    int _dirty;
    char *_real_data;
    
    Memo();
    Memo(char *, int, int);
    Memo(int, int);
    ~Memo();
  };
  /** @endcond never */
    
  mutable const char *_data;	// mutable for c_str()
  mutable int _length;
  mutable Memo *_memo;
  
  inline String(const char *, int, Memo *);
  
  inline void assign(const String &) const;
  void assign(const char *, int);
  inline void deref() const;
  void make_out_of_memory();
  
  static Memo *null_memo;
  static Memo *permanent_memo;
  static Memo *oom_memo;
  static String *null_string_p;
  static String *oom_string_p;
  static const char oom_string_data;
  
  static String claim_string(char *, int, int); // claim memory

  friend class String::Initializer;
  friend class StringAccum;
  
};


inline
String::String(const char *data, int length, Memo *memo)
  : _data(data), _length(length), _memo(memo)
{
  _memo->_refcount++;
}

inline void
String::assign(const String &str) const
{
  _data = str._data;
  _length = str._length;
  _memo = str._memo;
  _memo->_refcount++;
}

inline void
String::deref() const
{
  if (--_memo->_refcount == 0)
    delete _memo;
}

/** @brief Create an empty String (with length 0). */
inline
String::String()
  : _data(null_memo->_real_data), _length(0), _memo(null_memo)
{
  _memo->_refcount++;
}

/** @brief Create a String containing a copy of the C string @a cstr.
 * @param cstr a null-terminated C string.
 * @return A String containing the characters of @a cstr, up to but not
 * including the terminating null character.
 *
 * If @a cstr equals String::out_of_memory_data(), returns an
 * out-of-memory string.
 */
inline
String::String(const char *cstr)
{
  assign(cstr, -1);
}

/** @brief Create a String containing a copy of the first @a len characters of
 * string @a s.
 * @param s a string.
 * @param len number of characters to take from @a cc.  If @a len @< 0, then
 * takes @c strlen(@a s) characters.
 * @return A String containing @a len characters of @a s.
 *
 * If @a s equals String::out_of_memory_data(), returns an
 * out-of-memory string.
 */
inline
String::String(const char *s, int len)
{
  assign(s, len);
}

/** @brief Create a String containing a copy of the characters from @a begin
 * to @a end.
 * @param begin first character in string (begin iterator).
 * @param end pointer one past last character in string (end iterator).
 * @return A String containing the characters from @a begin to @a end.
 *
 * Returns a null string if @a begin @> @a end. 
 * If @a begin equals String::out_of_memory_data(), returns an
 * out-of-memory string.
 */
inline
String::String(const char *begin, const char *end)
{
  assign(begin, (end > begin ? end - begin : 0));
}

/** @brief Create a String equal to "true" or "false" depending on the
 * value of @a b.
 * @param b a boolean variable.
 */
inline
String::String(bool b)
  : _data(b ? "true" : "false"), _length(b ? 4 : 5), _memo(permanent_memo)
{
  _memo->_refcount++;
}

/** @brief Create a String containing the single character @a c.
 * @param c a character.
 */
inline
String::String(char c)
{
  assign(&c, 1);
}

/** @brief Create a String containing the single character @a c.
 * @param c an unsigned character.
 */
inline
String::String(unsigned char c)
{
  assign(reinterpret_cast<char *>(&c), 1);
}

/** @brief Create a String containing a copy of the String @a str.
 * @param str a String.
 */
inline
String::String(const String &str)
{
  assign(str);
}

/** @brief Destroy a String, freeing memory if necessary. */
inline
String::~String()
{
  deref();
}

/** @brief Return the string's length. */
inline int
String::length() const
{
  return _length;
}

/** @brief Return a pointer to the string's data.
 *
 * Only the first length() characters are valid, and the string data might not
 * be null-terminated. */
inline const char *
String::data() const
{
  return _data;
}

/** @brief Return an iterator for the first character in the string.
 *
 * String iterators are simply pointers into string data, so they are quite
 * efficient.  @sa String::data */
inline String::const_iterator
String::begin() const
{
  return _data;
}

/** @brief Return an iterator for the end of the string.
 *
 * The return value points one character beyond the last character in the
 * string. */
inline String::const_iterator
String::end() const
{
  return _data + _length;
}

/** @brief Returns true iff the string is nonempty. */
inline
String::operator bool() const
{
  return _length != 0;
}

/** @overload
 */
inline
String::operator bool()
{
  return _length != 0;
}
  
/** @brief Returns the @a i th character in the string.
 *
 * Does not check bounds.
 * @sa String::at */
inline char
String::operator[](int i) const
{
  return _data[i];
}

/** @brief Returns the @a i th character in the string.
 *
 * Checks bounds: an assertion will fail if @a i is less than 0 or not less
 * than length().
 * @sa String::operator[]
 */
inline char
String::at(int i) const
{
  assert(i >= 0 && i < _length);
  return _data[i];
}

/** @brief Returns the first character in the string.
 *
 * Does not do check bounds.  Same as (*this)[0]. */
inline char
String::front() const
{
  return _data[0];
}

/** @brief Returns the last character in the string.
 *
 * Does not check bounds.  Same as (*this)[length() - 1]. */
inline char
String::back() const
{
  return _data[_length - 1];
}

/** @brief Return true iff the String's data is shared or immutable. */
inline bool
String::data_shared() const
{
  return !_memo->_capacity || _memo->_refcount != 1;
}

/** @brief Return an empty String.
 *
 * Returns a global constant, so it's quicker than String::String().
 */
inline const String &
String::empty_string()
{
  return *null_string_p;
}

/** @brief Return a String that directly references the character data in
 * [@a begin, @a end).
 * @param begin pointer to the first character in the character data.
 * @param end pointer one beyond the last character in the character data.
 *
 * This function is suitable for static constant strings whose data is known
 * to stay around forever, such as C string constants.  Returns a null string
 * if @a begin @> @a end.
 */
inline String
String::stable_string(const char *begin, const char *end)
{
    if (begin < end)
	return String::stable_string(begin, end - begin);
    else
	return String();
}

/** @brief Return a substring of the current string starting at @a begin and
 * ending before @a end.
 * @param begin pointer to the first character in the desired substring.
 * @param end pointer one beyond the last character in the desired substring.
 *
 * Returns a null string if @a begin @> @a end, or if @a begin or @a end is
 * out of range (i.e., either less than this->begin() or greater than
 * this->end()).
 */
inline String
String::substring(const char *begin, const char *end) const
{
    if (begin < end && begin >= _data && end <= _data + _length)
	return String(begin, end - begin, _memo);
    else
	return String();
}

/** @brief Return the suffix of the current string starting at index @a pos.
 *
 * Same as String::substring(@a pos, INT_MAX).
 */
inline String
String::substring(int pos) const
{
  return substring(pos, _length);
}

/** @brief Compare two strings.
 * @param a first string to compare
 * @param b second string to compare
 *
 * Returns 0 if @a a == @a b, negative if @a a @< @a b in lexicographic
 * order, and positive if @a a @> @a b in lexicographic order.
 */
inline int
String::compare(const String &a, const String &b)
{
  return a.compare(b);
}

/** @brief Compare this string with string @a str.
 *
 * Same as String::compare(*this, @a str).
 * @sa String::compare(const String &a, const String &b) */
inline int
String::compare(const String &str) const
{
  return compare(str._data, str._length);
}

/** @relates String
 * @brief Compares two strings for equality.
 *
 * Returns true iff the two operands have the same lengths and the same
 * characters in the same order.  At most one of the operands can be a
 * null-terminated C string.
 * @sa String::compare
 */
inline bool
operator==(const String &a, const String &b)
{
  return a.equals(b.data(), b.length());
}

/** @relates String */
inline bool
operator==(const char *a, const String &b)
{
  return b.equals(a, -1);
}

/** @relates String */
inline bool
operator==(const String &a, const char *b)
{
  return a.equals(b, -1);
}

/** @relates String
 * @brief Compare two Strings for inequality.
 *
 * Returns true iff !(@a a == @a b).  At most one of the operands can be a
 * null-terminated C string. */
inline bool
operator!=(const String &a, const String &b)
{
  return !a.equals(b.data(), b.length());
}

/** @relates String */
inline bool
operator!=(const char *a, const String &b)
{
  return !b.equals(a, -1);
}

/** @relates String */
inline bool
operator!=(const String &a, const char *b)
{
  return !a.equals(b, -1);
}

/** @relates String
 * @brief Compare two Strings.
 *
 * Returns true iff @a a @< @a b in lexicographic order.
 * @sa String::compare
 */
inline bool
operator<(const String &a, const String &b)
{
  return a.compare(b.data(), b.length()) < 0;
}

/** @relates String
 * @brief Compare two Strings.
 *
 * Returns true iff @a a @<= @a b in lexicographic order.
 * @sa String::compare
 */
inline bool
operator<=(const String &a, const String &b)
{
  return a.compare(b.data(), b.length()) <= 0;
}

/** @relates String
 * @brief Compare two Strings.
 *
 * Returns true iff @a a @> @a b in lexicographic order.
 * @sa String::compare
 */
inline bool
operator>(const String &a, const String &b)
{
  return a.compare(b.data(), b.length()) > 0;
}

/** @relates String
 * @brief Compare two Strings.
 *
 * Returns true iff @a a @>= @a b in lexicographic order.
 * @sa String::compare
 */
inline bool
operator>=(const String &a, const String &b)
{
  return a.compare(b.data(), b.length()) >= 0;
}

/** @brief Makes this string a copy of @a str. */
inline String &
String::operator=(const String &str)
{
  if (&str != this) {
    deref();
    assign(str);
  }
  return *this;
}

/** @brief Make this string a copy of the C string @a cstr. */
inline String &
String::operator=(const char *cstr)
{
  deref();
  assign(cstr, -1);
  return *this;
}

/** @brief Appends the data from @a begin to @a end to the end of this string.
 *
 * Does nothing if @a begin @> @a end. */
inline void
String::append(const char *begin, const char *end)
{
  if (begin < end)
    append(begin, end - begin);
}

/** @brief Append a copy of @a str to the end of this string.
 *
 * Returns the result. */
inline String &
String::operator+=(const String &str)
{
  append(str._data, str._length);
  return *this;
}

/** @brief Append a copy of the C string @a cstr to the end of this string.
 *
 * Returns the result. */
inline String &
String::operator+=(const char *cstr)
{
  append(cstr, -1);
  return *this;
}

/** @brief Append the character @a c to the end of this string.
 *
 * Returns the result. */
inline String &
String::operator+=(char c)
{
  append(&c, 1);
  return *this;
}

/** @relates String
 * @brief Concatenate the operands and return the result.
 *
 * At most one of the two operands can be a null-terminated C string. */
inline String
operator+(String a, const String &b)
{
  a += b;
  return a;
}

/** @relates String */
inline String
operator+(String a, const char *b)
{
  a.append(b, -1);
  return a;
}

/** @relates String */
inline String
operator+(const char *a, const String &b)
{
  String s1(a);
  s1 += b;
  return s1;
}

/** @relates String
 * @brief Concatenate the operands and return the result.
 *
 * The second operand is a single character. */
inline String
operator+(String a, char b)
{
  a.append(&b, 1);
  return a;
}

/** @relates String */
int hashcode(const String &);

/** @brief Returns true iff this is an out-of-memory string. */
inline bool
String::out_of_memory() const
{
  return _data == &oom_string_data;
}

/** @brief Return a reference to an out-of-memory String. */
inline const String &
String::out_of_memory_string()
{
  return *oom_string_p;
}

/** @brief Return the data pointer used for out-of-memory strings.
 *
 * The returned value may be dereferenced; it points to a null character. 
 */
inline const char *
String::out_of_memory_data()
{
  return &oom_string_data;
}

// find methods

inline const char *rfind(const char *begin, const char *end, char c)
{
  for (const char *bb = end - 1; bb >= begin; bb--)
    if (*bb == c)
      return bb;
  return end;
}

inline const char *find(const String &s, char c)
{
  return find(s.begin(), s.end(), c);
}

CLICK_ENDDECLS
#endif
