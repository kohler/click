#ifndef STRING_HH
#define STRING_HH
#ifdef HAVE_PERMSTRING
# include "permstr.hh"
#endif

class String { public:
  
  // Call static_initialize() before any function which might deal with
  // Strings, and declare a String::Initializer in any file in which you
  // declare static global Strings.
  struct Initializer { Initializer(); };
  friend class String::Initializer;
  static void static_initialize();
  static void static_cleanup();
  
  String();
  String(const String &s);
  String(const char *cc)		{ assign(cc, -1); }
  String(const char *cc, int l)		{ assign(cc, l); }
#ifdef HAVE_PERMSTRING
  String(PermString p);
#endif
  explicit String(char);
  explicit String(unsigned char);
  explicit String(int);
  explicit String(unsigned);
  explicit String(unsigned long);
  explicit String(unsigned long long);
  ~String();
  
  static const String &null_string()	{ return *null_string_p; }
  static String claim_string(const char *, int = -1); // claim memory
  
  static int out_of_memory_count();
  
  int length() const			{ return _length; }
  const char *data() const		{ return _data; }
  char *mutable_data();
  char *mutable_c_str();
  
  operator bool()			{ return _length != 0; }
  operator bool() const			{ return _length != 0; }
  
#ifdef HAVE_PERMSTRING
  operator PermString()			{ return PermString(_data, _length); }
  operator PermString() const		{ return PermString(_data, _length); }
#endif
  
  const char *cc();			// pointer returned is semi-transient
  operator const char *()		{ return cc(); }
  
  char operator[](int e) const		{ return _data[e]; }
  char back() const			{ return _data[_length-1]; }
  int find_left(int c, int start = 0) const;
  int find_left(const String &s, int start = 0) const;
  int find_right(int c, int start = 0x7FFFFFFF) const;
  
  int hashcode() const;
  
  bool equals(const char *, int) const;
  // bool operator==(const String &, const String &);
  // bool operator==(const String &, const char *);
  // bool operator!=(const String &, const String &);
  // etc.
  
  String substring(int, int) const;
  String substring(int left) const	{ return substring(left, _length); }

  String lower() const;
  String upper() const;
  
  String &operator=(const String &);
  String &operator=(const char *);
#ifdef HAVE_PERMSTRING
  String &operator=(PermString);
#endif

  void append(const char *, int);
  String &operator+=(const String &);
  String &operator+=(const char *);
  String &operator+=(char);
#ifdef HAVE_PERMSTRING
  String &operator+=(PermString p);
#endif

  // String operator+(String, const String &);
  // String operator+(String, const char *);
  // String operator+(const char *, String);
  // String operator+(String, PermString);
  // String operator+(PermString, String);
  // String operator+(PermString, const char *);
  // String operator+(const char *, PermString);
  // String operator+(PermString, PermString);
  // String operator+(String, char);
  
 private:
   
  struct Memo {
    int _refcount;
    int _capacity;
    int _dirty;
    char *_real_data;
    
    Memo();
    Memo(int, int);
    ~Memo();
  };
  
  const char *_data;
  int _length;
  Memo *_memo;
  
  String(const char *, int, Memo *);
  
  void assign(const String &);
  void assign(const char *, int);
#ifdef HAVE_PERMSTRING
  void assign(PermString);
#endif
  void deref();
  void out_of_memory();
  
  static Memo *null_memo;
  static Memo *permanent_memo;
  static String *null_string_p;
  
};


inline
String::String(const char *data, int length, Memo *memo)
  : _data(data), _length(length), _memo(memo)
{
  _memo->_refcount++;
}

inline void
String::assign(const String &s)
{
  _data = s._data;
  _length = s._length;
  _memo = s._memo;
  _memo->_refcount++;
}

inline void
String::deref()
{
  if (--_memo->_refcount == 0) delete _memo;
}

inline
String::String()
  : _data(null_memo->_real_data), _length(0), _memo(null_memo)
{
  _memo->_refcount++;
}

inline
String::String(char c)
{
  assign(&c, 1);
}

inline
String::String(unsigned char c)
{
  assign(reinterpret_cast<char *>(&c), 1);
}

inline
String::String(const String &s)
{
  assign(s);
}

inline
String::~String()
{
  deref();
}

inline bool
operator==(const String &s1, const String &s2)
{
  return s1.equals(s2.data(), s2.length());
}

inline bool
operator==(const char *cc1, const String &s2)
{
  return s2.equals(cc1, -1);
}

inline bool
operator==(const String &s1, const char *cc2)
{
  return s1.equals(cc2, -1);
}

inline bool
operator!=(const String &s1, const String &s2)
{
  return !s1.equals(s2.data(), s2.length());
}

inline bool
operator!=(const char *cc1, const String &s2)
{
  return !s2.equals(cc1, -1);
}

inline bool
operator!=(const String &s1, const char *cc2)
{
  return !s1.equals(cc2, -1);
}

inline String &
String::operator=(const String &s)
{
  if (&s != this) {
    deref();
    assign(s);
  }
  return *this;
}

inline String &
String::operator=(const char *cc)
{
  deref();
  assign(cc, -1);
  return *this;
}

inline String &
String::operator+=(const String &s)
{
  append(s._data, s._length);
  return *this;
}

inline String &
String::operator+=(const char *cc)
{
  append(cc, -1);
  return *this;
}

inline String &
String::operator+=(char c)
{
  append(&c, 1);
  return *this;
}

inline String
operator+(String s1, const String &s2)
{
  s1 += s2;
  return s1;
}

inline String
operator+(String s1, const char *cc2)
{
  s1.append(cc2, -1);
  return s1;
} 

inline String
operator+(const char *cc1, const String &s2)
{
  String s1(cc1);
  s1 += s2;
  return s1;
} 

inline String
operator+(String s1, char c2)
{
  s1.append(&c2, 1);
  return s1;
} 

#ifdef HAVE_PERMSTRING

inline void
String::assign(PermString p)
{
  assert(p && "null PermString");
  _data = p.cc();
  _length = p.length();
  _memo = permanent_memo;
  _memo->_refcount++;
}

inline
String::String(PermString p)
{
  assign(p);
}

inline bool
operator==(PermString p1, const String &s2)
{
  return p1 && s2.equals(p1.cc(), p1.length());
}

inline bool
operator==(const String &s1, PermString p2)
{
  return p2 && s1.equals(p2.cc(), p2.length());
}

inline bool
operator!=(PermString p1, const String &s2)
{
  return !s2.equals(p1.cc(), p1.length());
}

inline bool
operator!=(const String &s1, PermString p2)
{
  return !s1.equals(p2.cc(), p2.length());
}

inline String &
String::operator=(PermString p)
{
  deref();
  assign(p);
  return *this;
}

inline String &
String::operator+=(PermString p)
{
  append(p.cc(), p.length());
  return *this;
}

inline String
operator+(String s1, PermString p2)
{
  s1.append(p2.cc(), p2.length());
  return s1;
} 

inline String
operator+(PermString p1, String s2)
{
  String s1(p1);
  return s1 + s2;
} 

inline String
operator+(PermString p1, const char *cc2)
{
  String s1(p1);
  return s1 + cc2;
} 

inline String
operator+(const char *cc1, PermString p2)
{
  String s1(cc1);
  return s1 + p2;
} 

inline String
operator+(PermString p1, PermString p2)
{
  String s1(p1);
  return s1 + p2;
}

#endif

#endif
