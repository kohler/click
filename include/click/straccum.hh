#ifndef STRACCUM_HH
#define STRACCUM_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <assert.h>
#ifdef __KERNEL__
# include <asm/string.h>
#else
# include <string.h>
#endif

class StringAccum { public:
  
  StringAccum()				: _s(0), _len(0), _cap(0) { }
  explicit StringAccum(int);
  ~StringAccum()			{ delete[] _s; }

  const char *cc();
  char *data() const			{ return (char *)_s; }
  int length() const			{ return _len; }
  
  void clear()				{ _len = 0; }
  
  char *reserve(int);
  void forward(int f)			{ _len += f; assert(_len <= _cap); }
  char *extend(int);
  
  void append(char);
  void append(unsigned char);
  void append(const char *, int);
  
  void pop(int n = 1)			{ if (_len >= n) _len -= n; }
  
  void take(unsigned char *&s, int &l)	{ s = _s; l = _len; erase(); }
  unsigned char *take_bytes();
  char *take();
  String take_string();

  // see also operator<< declarations below
  
  // STRING OPERATIONS
  
  char operator[](int i) const	{ assert(i>=0 && i<_len); return (char)_s[i]; }
  char &operator[](int i)	{ assert(i>=0 && i<_len); return (char &)_s[i]; }

 private:
  
  unsigned char *_s;
  int _len;
  int _cap;
  
  bool grow(int);
  void erase()				{ _s = 0; _len = 0; _cap = 0; }

  StringAccum(const StringAccum &);
  StringAccum &operator=(const StringAccum &);
  
};

StringAccum &operator<<(StringAccum &, char);
StringAccum &operator<<(StringAccum &, unsigned char);
StringAccum &operator<<(StringAccum &, const char *);
StringAccum &operator<<(StringAccum &, const String &);
StringAccum &operator<<(StringAccum &, const StringAccum &);
#ifdef HAVE_PERMSTRING
StringAccum &operator<<(StringAccum &, PermString);
#endif

StringAccum &operator<<(StringAccum &, short);
StringAccum &operator<<(StringAccum &, unsigned short);
StringAccum &operator<<(StringAccum &, int);
StringAccum &operator<<(StringAccum &, unsigned);
StringAccum &operator<<(StringAccum &, long);
StringAccum &operator<<(StringAccum &, unsigned long);
StringAccum &operator<<(StringAccum &, unsigned long long);
#ifndef __KERNEL__
StringAccum &operator<<(StringAccum &, double);
#endif

struct timeval;
StringAccum &operator<<(StringAccum &, const struct timeval &);


inline
StringAccum::StringAccum(int cap)
  : _s(new unsigned char[cap]), _len(0), _cap(cap)
{
}

inline void
StringAccum::append(unsigned char c)
{
  if (_len < _cap || grow(_len))
    _s[_len++] = c;
}

inline void
StringAccum::append(char c)
{
  append(static_cast<unsigned char>(c));
}

inline void
StringAccum::append(const char *s, int len)
{
  if (char *x = extend(len))
    memcpy(x, s, len);
}

inline char *
StringAccum::reserve(int hm)
{
  if (_len + hm <= _cap || grow(_len + hm))
    return (char *)(_s + _len);
  else
    return 0;
}

inline char *
StringAccum::extend(int amt)
{
  char *c = reserve(amt);
  if (c) _len += amt;
  return c;
}

inline unsigned char *
StringAccum::take_bytes()
{
  unsigned char *str = _s;
  erase();
  return str;
}

inline char *
StringAccum::take()
{
  return (char *)take_bytes();
}

inline StringAccum &
operator<<(StringAccum &sa, char c)
{
  sa.append(c);
  return sa;
}

inline StringAccum &
operator<<(StringAccum &sa, unsigned char c)
{
  sa.append(c);
  return sa;
}

inline StringAccum &
operator<<(StringAccum &sa, short i)
{
  return sa << static_cast<long>(i);
}

inline StringAccum &
operator<<(StringAccum &sa, unsigned short u)
{
  return sa << static_cast<unsigned long>(u);
}

inline StringAccum &
operator<<(StringAccum &sa, int i)
{
  return sa << static_cast<long>(i);
}

inline StringAccum &
operator<<(StringAccum &sa, unsigned u)
{
  return sa << static_cast<unsigned long>(u);
}

#ifdef HAVE_PERMSTRING
inline StringAccum &
operator<<(StringAccum &sa, PermString s)
{
  sa.append(s.cc(), s.length());
  return sa;
}
#endif

inline StringAccum &
operator<<(StringAccum &sa, const String &s)
{
  sa.append(s.data(), s.length());
  return sa;
}

inline StringAccum &
operator<<(StringAccum &sa, const StringAccum &sb)
{
  sa.append(sb.data(), sb.length());
  return sa;
}

#endif
