/*
 * region.{cc,hh} -- Rectangular region class
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include <stdio.h>
#include "region.hh"

static double 
max(double a, double b) 
{ return a > b ? a : b; }

static double 
min(double a, double b) 
{ return a > b ? b : a; }

String
RectRegion::s() 
{
  char buf[200];
  if (empty())
    snprintf(buf, 200, "<empty rgn>");
  else
    snprintf(buf, 200, "(%f, %f) +%f +%f", _x, _y, _w, _h);
  return String(buf);
}

RectRegion
RectRegion::intersect(RectRegion &r)
{
  RectRegion ret; // empty default region
  if (empty() || r.empty()) 
    return r;
  if (!(r._x > _x + _w ||
	r._x + r._w < _x ||
	r._y > _y + _h ||
	r._y + r._h < _y)) {
    ret._x = max(_x, r._x);
    ret._y = max(_y, r._y);
    ret._w = min(_x + _w, r._x + r._w) - ret._x;
    ret._h = min(_y + _h, r._h + r._h) - ret._y;
  }
  return ret;  
}


RectRegion
RectRegion::expand(double l)
{
  RectRegion r(*this);
  r._x -= l;
  r._y -= l;
  r._w += 2 * l;
  r._h += 2 * l;
  return r;
}

ELEMENT_REQUIRES(userlevel)
ELEMENT_PROVIDES(RectRegion)
