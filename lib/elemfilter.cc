/*
 * elemfilter.{cc,hh} -- element filters
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
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


CastElementFilter::CastElementFilter(const String &what)
  : _what(what)
{
}

bool
CastElementFilter::match(Element *f)
{
  return f->cast(_what) != 0;
}
