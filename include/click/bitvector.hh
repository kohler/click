// -*- c-basic-offset: 2; related-file-name: "../../lib/bitvector.cc" -*-
#ifndef CLICK_BITVECTOR_HH
#define CLICK_BITVECTOR_HH
#include <assert.h>
#include <click/glue.hh>

class Bitvector { public:

  class Bit;

  Bitvector()			: _max(-1), _data(&_f0), _f0(0), _f1(0) { }
  explicit Bitvector(int);
  explicit Bitvector(unsigned);
  explicit Bitvector(bool b)	: _max(0), _data(&_f0), _f0(b), _f1(0) { }
  Bitvector(int, bool);
  Bitvector(unsigned, bool);
  Bitvector(const Bitvector &);
  ~Bitvector()			{ if (_max >= INLINE_BITS) delete[] _data; }
  
  int size() const		{ return _max + 1; }
  
  bool zero() const;
  operator bool() const		{ return !zero(); }
  
  Bit operator[](int);
  bool operator[](int) const;
  Bit force_bit(int);
  
  void clear();
  void resize(int n)	{ if (n > INLINE_BITS) resize_x(n, true); _max = n-1; }
  
  bool operator==(const Bitvector &) const;
  bool operator!=(const Bitvector &) const;
  
  Bitvector operator~() const;
  Bitvector operator&(const Bitvector &) const;
  Bitvector operator|(const Bitvector &o) const;
  Bitvector operator^(const Bitvector &o) const;
  Bitvector operator-(const Bitvector &o) const;
  
  Bitvector &operator=(const Bitvector &);
  void negate();
  Bitvector &operator&=(const Bitvector &);
  Bitvector &operator|=(const Bitvector &);
  Bitvector &operator^=(const Bitvector &);
  Bitvector &operator-=(const Bitvector &);
  Bitvector &assign(int, bool);
  
  void or_at(const Bitvector &, int);
  bool nonzero_intersection(const Bitvector &);
  
  // exposing the implementation
  int u_max() const		{ return (_max < 0 ? -1 : _max>>5); }
  unsigned *u_data()		{ return _data; }
  
 private:

  static const int INLINE_BITS = 64;
  static const int INLINE_UNSIGNEDS = 2;
  
  int _max;
  unsigned *_data;
  unsigned _f0;
  unsigned _f1;
  
  void finish_copy_constructor(const Bitvector &);
  void clear_last();
  void resize_x(int, bool);
  
};

class Bitvector::Bit { public:

  Bit(unsigned &p, int off)		: _p(p), _mask(1U<<off) { }
  
  operator bool() const			{ return (_p & _mask) != 0; }
  operator bool()			{ return (_p & _mask) != 0; }
  
  bool operator=(bool b);
  bool operator=(const Bit &);
  
 private:
  
  unsigned &_p;
  unsigned _mask;
  
};


inline
Bitvector::Bitvector(int n)
  : _max(n - 1), _data(&_f0), _f0(0), _f1(0)
{
  if (n > INLINE_BITS) resize_x(n, false);
}

inline
Bitvector::Bitvector(unsigned n)
  : _max(n - 1), _data(&_f0), _f0(0), _f1(0)
{
  if (n > static_cast<unsigned>(INLINE_BITS)) resize_x(n, false);
}

inline
Bitvector::Bitvector(int n, bool b)
  : _max(n - 1), _data(&_f0), _f0(0), _f1(0)
{
  if (n > INLINE_BITS) resize_x(n, false);
  if (b) assign(n, b);
}

inline
Bitvector::Bitvector(unsigned n, bool b)
  : _max(n - 1), _data(&_f0), _f0(0), _f1(0)
{
  if (n > static_cast<unsigned>(INLINE_BITS)) resize_x(n, false);
  if (b) assign(n, b);
}

inline
Bitvector::Bitvector(const Bitvector &o)
  : _max(o._max), _data(&_f0), _f0(o._f0), _f1(o._f1)
{
  if (_max >= INLINE_BITS) finish_copy_constructor(o);
}

inline bool
Bitvector::Bit::operator=(bool b)
{
  if (b) _p |= _mask;
  else _p &= ~_mask;
  return b;
}

inline bool
Bitvector::Bit::operator=(const Bit &o)
{
  return *this = (bool)o;
}

inline Bitvector::Bit
Bitvector::operator[](int i)
{
  assert(i >= 0 && i <= _max);
  return Bit(_data[i>>5], i&31);
}

inline Bitvector::Bit
Bitvector::force_bit(int i)
{
  assert(i >= 0);
  if (i > _max)
    resize(i + 1);
  return Bit(_data[i>>5], i&31);
}

inline bool
Bitvector::operator[](int i) const
{
  assert(i >= 0 && i <= _max);
  return (_data[i>>5] & (1<<(i&31))) != 0;
}

inline bool
Bitvector::operator==(const Bitvector &o) const
{
  if (_max != o._max)
    return false;
  else if (_max < INLINE_BITS)
    return _f0 == o._f0 && _f1 == o._f1;
  else
    return memcmp(_data, o._data, (u_max()+1)*4) == 0;
}

inline bool
Bitvector::operator!=(const Bitvector &o) const
{
  return !(*this == o);
}

inline Bitvector &
Bitvector::operator-=(const Bitvector &o)
{
  return *this &= ~o;
}

inline Bitvector
Bitvector::operator~() const
{
  Bitvector m = *this; m.negate(); return m;
}

inline Bitvector
Bitvector::operator&(const Bitvector &o) const
{
  Bitvector m = *this; m &= o; return m;
}

inline Bitvector
Bitvector::operator|(const Bitvector &o) const
{
  Bitvector m = *this; m |= o; return m;
}

inline Bitvector
Bitvector::operator^(const Bitvector &o) const
{
  Bitvector m = *this; m ^= o; return m;
}

inline Bitvector
Bitvector::operator-(const Bitvector &o) const
{
  return *this & ~o;
}

#endif
