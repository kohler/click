/*
 * elementt.{cc,hh} -- tool definition of element
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
#include "elementt.hh"
#include <stdlib.h>

ElementT::ElementT()
  : type(-1), tunnel_input(-1), tunnel_output(-1), flags(0)
{
}

ElementT::ElementT(const String &n, int type_index, const String &config,
		   const String &lm)
  : type(type_index), name(n), configuration(config),
    tunnel_input(-1), tunnel_output(-1), landmark(lm), flags(0)
{
}


int
Hookup::index_in(const Vector<Hookup> &v, int start) const
{
  int size = v.size();
  for (int i = start; i < size; i++)
    if (v[i] == *this)
      return i;
  return -1;
}

int
Hookup::force_index_in(Vector<Hookup> &v, int start) const
{
  int size = v.size();
  for (int i = start; i < size; i++)
    if (v[i] == *this)
      return i;
  v.push_back(*this);
  return size;
}

int
Hookup::sorter(const void *av, const void *bv)
{
  const Hookup *a = (const Hookup *)av, *b = (const Hookup *)bv;
  if (a->idx == b->idx)
    return a->port - b->port;
  else
    return a->idx - b->idx;
}

void
Hookup::sort(Vector<Hookup> &v)
{
  qsort(&v[0], v.size(), sizeof(Hookup), &sorter);
}


ElementClassT::ElementClassT()
  : _use_count(1)
{
}

void
ElementClassT::expand_compound(ElementT &, RouterT *, ErrorHandler *)
{
}

void
ElementClassT::compound_declaration_string(StringAccum &, const String &, const String &)
{
}
