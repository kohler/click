#ifndef STRACCUM_HH
#define STRACCUM_HH
#include "string.hh"
#include "glue.hh"
#include <assert.h>
#ifdef __KERNEL__
# include <asm/string.h>
#else
# include <string.h>
#endif

class StringAccum {
  
  unsigned char *_s;
  int _len;
  int _cap;
  
  bool grow(int);
  void erase()				{ _s = 0; _len = 0; _cap = 0; }

  StringAccum(const StringAccum &);
  StringAccum &operator=(const StringAccum &);
  
 public:
  
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
  
  void push(unsigned char);
  void push(char);
  void push(const char *, int);
  
  void pop(int n = 1)			{ if (_len >= n) _len -= n; }
  
  void take(unsigned char *&s, int &l)	{ s = _s; l = _len; erase(); }
  unsigned char *take_bytes();
  char *take();
  String take_string();
  
  StringAccum &operator<<(char c)		{ push(c); return *this; }
  StringAccum &operator<<(unsigned char c)	{ push(c); return *this; }
  StringAccum &operator<<(const char *);
#ifdef HAVE_PERMSTRING
  StringAccum &operator<<(PermString);
#endif
  StringAccum &operator<<(const String &);
  StringAccum &operator<<(int);
  StringAccum &operator<<(unsigned);
  StringAccum &operator<<(long);
  StringAccum &operator<<(unsigned long);
#ifndef __KERNEL__
  StringAccum &operator<<(double);
#endif
  StringAccum &operator<<(unsigned long long);
  
  // STRING OPERATIONS
  
  char operator[](int i) const	{ assert(i>=0 && i<_len); return (char)_s[i]; }
  char &operator[](int i)	{ assert(i>=0 && i<_len); return (char &)_s[i]; }
  
};


inline
StringAccum::StringAccum(int cap)
  : _s(new unsigned char[cap]), _len(0), _cap(cap)
{
}

inline void
StringAccum::push(unsigned char c)
{
  if (_len < _cap || grow(_len))
    _s[_len++] = c;
}

inline void
StringAccum::push(char c)
{
  push(static_cast<unsigned char>(c));
}

inline void
StringAccum::push(const char *s, int len)
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
StringAccum::operator<<(int i)
{
  return *this << static_cast<long>(i);
}

inline StringAccum &
StringAccum::operator<<(unsigned u)
{
  return *this << static_cast<unsigned long>(u);
}

#endif
