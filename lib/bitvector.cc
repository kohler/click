/*
 * bitvector.{cc,hh} -- generic bit vector class
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "bitvector.hh"

void
Bitvector::finish_copy_constructor(const Bitvector &o)
{
  int nn = u_max();
  _data = new unsigned[nn + 1];
  for (int i = 0; i <= nn; i++)
    _data[i] = o._data[i];
}

void
Bitvector::clear()
{
  int nn = u_max();
  for (int i = 0; i <= nn; i++)
    _data[i] = 0;
}

bool
Bitvector::zero() const
{
  int nn = u_max();
  for (int i = 0; i <= nn; i++)
    if (_data[i])
      return false;
  return true;
}

void
Bitvector::resize(int n, bool valid_n)
{
  if (n <= Contains) {
    if (_max >= Contains)
      delete[] _data;
    _data = &_f0;
    return;
  }
  
  int want_u = ((n-1)/32) + 1;
  int have_u = (valid_n ? u_max() + 1 : 2);
  if (want_u == have_u) return;
  
  unsigned *new_data = new unsigned[want_u];
  for (int i = 0; i < have_u; i++)
    new_data[i] = _data[i];
  for (int i = have_u; i < want_u; i++)
    new_data[i] = 0;
  if (valid_n && _max >= Contains)
    delete[] _data;
  _data = new_data;
}

void
Bitvector::clear_last()
{
  if ((_max&0x1F) != 0x1F) {
    unsigned mask = (1U << ((_max&0x1F)+1)) - 1;
    _data[_max>>5] &= mask;
  } else if (_max < 0)
    _data[0] = 0;
}

Bitvector &
Bitvector::operator=(const Bitvector &o)
{
  if (&o != this) {
    resize(o._max + 1);
    _max = o._max;
    int copy = u_max();
    if (copy < ContainsU-1) copy = ContainsU-1;
    for (int i = 0; i <= copy; i++)
      _data[i] = o._data[i];
  }
  return *this;
}

Bitvector &
Bitvector::assign(int n, bool value)
{
  resize(n);
  _max = n - 1;
  unsigned bits = (value ? 0xFFFFFFFFU : 0U);
  int copy = u_max();
  if (copy < ContainsU-1) copy = ContainsU-1;
  for (int i = 0; i <= copy; i++)
    _data[i] = bits;
  if (value) clear_last();
  return *this;
}

void
Bitvector::negate()
{
  int nn = u_max();
  unsigned *data = _data;
  for (int i = 0; i <= nn; i++)
    data[i] = ~data[i];
  clear_last();
}

Bitvector &
Bitvector::operator&=(const Bitvector &o)
{
  assert(o._max == _max);
  int nn = u_max();
  unsigned *data = _data, *o_data = o._data;
  for (int i = 0; i <= nn; i++)
    data[i] &= o_data[i];
  return *this;
}

Bitvector &
Bitvector::operator|=(const Bitvector &o)
{
  assert(o._max == _max);
  int nn = u_max();
  unsigned *data = _data, *o_data = o._data;
  for (int i = 0; i <= nn; i++)
    data[i] |= o_data[i];
  return *this;
}

Bitvector &
Bitvector::operator^=(const Bitvector &o)
{
  assert(o._max == _max);
  int nn = u_max();
  unsigned *data = _data, *o_data = o._data;
  for (int i = 0; i <= nn; i++)
    data[i] ^= o_data[i];
  return *this;
}

void
Bitvector::or_at(const Bitvector &o, int offset)
{
  assert(offset >= 0 && offset + o._max <= _max);
  unsigned bits_1st = offset&0x1F;
  int my_pos = offset>>5;
  int o_pos = 0;
  int my_u_max = u_max();
  int o_u_max = o.u_max();
  unsigned *data = _data;
  unsigned *o_data = o._data;
  
  while (true) {
    int val = o_data[o_pos];
    data[my_pos] |= (val << bits_1st);
    
    my_pos++;
    if (my_pos > my_u_max) break;
    
    if (bits_1st) data[my_pos] |= (val >> (32 - bits_1st));
    
    o_pos++;
    if (o_pos > o_u_max) break;
  }
}
