#ifndef ALIGNMENT_HH
#define ALIGNMENT_HH
#include <click/string.hh>

class Alignment {

  int _chunk;
  int _offset;

  Alignment(int a, int b, int)		: _chunk(a), _offset(b) { }
  
 public:

  Alignment()				: _chunk(0), _offset(0) { }
  Alignment(int, int);

  int chunk() const			{ return _chunk; }
  int offset() const			{ return _offset; }
  bool bad() const			{ return _chunk < 0; }
  bool empty() const			{ return _chunk == 0; }

  bool operator==(const Alignment &) const;
  bool operator!=(const Alignment &) const;
  bool operator<=(const Alignment &) const;

  Alignment &operator+=(int);
  Alignment &operator-=(int);
  Alignment &operator|=(const Alignment &);
  Alignment &operator&=(const Alignment &);

  String unparse() const;
  String s() const			{ return unparse(); }

};

inline
Alignment::Alignment(int c, int o)
  : _chunk(c), _offset(o)
{
  assert(c > 0 && o >= 0 && o < c);
}

inline bool
Alignment::operator==(const Alignment &o) const
{
  return _chunk == o._chunk && _offset == o._offset;
}

inline bool
Alignment::operator!=(const Alignment &o) const
{
  return _chunk != o._chunk || _offset != o._offset;
}

inline Alignment &
Alignment::operator-=(int off)
{
  return *this += (-off);
}

inline Alignment
operator+(Alignment a, int off)
{
  return a += off;
}

inline Alignment
operator-(Alignment a, int off)
{
  return a += (-off);
}

inline Alignment
operator|(Alignment a, const Alignment &b)
{
  return a |= b;
}

inline Alignment
operator&(Alignment a, const Alignment &b)
{
  return a &= b;
}

#endif
