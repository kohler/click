/*
 * elemfilter.{cc,hh} -- element filters
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "elemfilter.hh"

void
ElementFilter::filter(Vector<Element *> &v)
{
  Vector<Element *> nv;
  for (int i = 0; i < v.size(); i++)
    if (match(v[i]))
      nv.push_back(v[i]);
  v = nv;
}


IsaElementFilter::IsaElementFilter(const String &what)
  : _what(what)
{
}

bool
IsaElementFilter::match(Element *f)
{
  return f->is_a(_what);
}
