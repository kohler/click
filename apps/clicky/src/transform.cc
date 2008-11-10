/* transform.{cc,hh} -- planar affine transformations
 *
 * Copyright (c) 2000-2006 Eddie Kohler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include "transform.hh"
#include <click/straccum.hh>
#include <math.h>

affine::affine()
{
    _m[0] = _m[3] = 1;
    _m[1] = _m[2] = _m[4] = _m[5] = 0;
    _null = true;
}

affine::affine(const double m[6])
{
    _m[0] = m[0];
    _m[1] = m[1];
    _m[2] = m[2];
    _m[3] = m[3];
    _m[4] = m[4];
    _m[5] = m[5];
    check_null(0);
}

affine::affine(double m0, double m1, double m2,
	       double m3, double m4, double m5)
{
    _m[0] = m0;
    _m[1] = m1;
    _m[2] = m2;
    _m[3] = m3;
    _m[4] = m4;
    _m[5] = m5;
    check_null(0);
}

void
affine::check_null(double tolerance)
{
    _null = (fabs(_m[0] - 1) <= tolerance && fabs(_m[1]) <= tolerance
	     && fabs(_m[2]) <= tolerance && fabs(_m[3] - 1) <= tolerance
	     && fabs(_m[4]) <= tolerance && fabs(_m[5]) <= tolerance);
}


affine
affine::mapping(const point &from1, const point &to1,
		const point &from2, const point &to2)
{
    assert(from1 != from2);
    if (to1 == to2)
	return affine(0, 0, 0, 0, to1.x(), to1.y());
    point fromv = from2 - from1, tov = to2 - to1;
    // The following is equivalent to:
    //   double s = tov.length() / fromv.length();
    //   double theta = tov.angle() - fromv.angle();
    //   double a = s * cos(theta), b = -sqrt(s*s - a*a);
    // It's just a bit faster/more precise since it doesn't call trig.
    // (Probably the imprecision doesn't matter.)
    double froml2 = fromv.squared_length();
    double a = (tov.x() * fromv.x() + tov.y() * fromv.y()) / froml2;
    double b = (tov.x() * fromv.y() - tov.y() * fromv.x()) / froml2;
    return affine(a, -b, b, a,
		  to1.x() - a*from1.x() - b*from1.y(),
		  to1.y() + b*from1.x() - a*from1.y());
}


void
affine::scale(double x, double y)
{
    _m[0] *= x;
    _m[1] *= x;
    _m[2] *= y;
    _m[3] *= y;

    if (x != 1 || y != 1)
	_null = false;
}

void
affine::rotate(double r)
{
    double c = cos(r);
    double s = sin(r);

    double a = _m[0], b = _m[2];
    _m[0] = a*c + b*s;
    _m[2] = b*c - a*s;

    a = _m[1], b = _m[3];
    _m[1] = a*c + b*s;
    _m[3] = b*c - a*s;

    if (r != 0)
	_null = false;
}

void
affine::translate(double x, double y)
{
    _m[4] += _m[0]*x + _m[2]*y;
    _m[5] += _m[1]*x + _m[3]*y;

    if (x != 0 || y != 0)
	_null = false;
}

void
affine::shear(double s)
{
    *this *= affine(1, 0, s, 1, 0, 0);
}

affine
affine::transformed(const affine &t) const
{
    return affine(_m[0] * t._m[0] + _m[2] * t._m[1],
		  _m[1] * t._m[0] + _m[3] * t._m[1],
		  _m[0] * t._m[2] + _m[2] * t._m[3],
		  _m[1] * t._m[2] + _m[3] * t._m[3],
		  _m[0] * t._m[4] + _m[2] * t._m[5] + _m[4],
		  _m[1] * t._m[4] + _m[3] * t._m[5] + _m[5]);
}


void
affine::real_apply_to(point &p) const
{
    double x = p.x(), y = p.y();
    p._x = x*_m[0] + y*_m[2] + _m[4];
    p._y = x*_m[1] + y*_m[3] + _m[5];
}

point
affine::real_apply(const point &p) const
{
    return point(p.x()*_m[0] + p.y()*_m[2] + _m[4],
		 p.x()*_m[1] + p.y()*_m[3] + _m[5]);
}

#if 0
Bezier &
operator*=(Bezier &b, const affine &t)
{
    if (!t.null()) {
	b.mpoint(0) *= t;
	b.mpoint(1) *= t;
	b.mpoint(2) *= t;
	b.mpoint(3) *= t;
    }
    return b;
}

Bezier
operator*(const Bezier &b, const affine &t)
{
    return (t.null()
	    ? b
	    : Bezier(b.point(0) * t, b.point(1) * t, b.point(2) * t, b.point(3) * t));
}
#endif

String
affine::unparse() const
{
    StringAccum sa;
    sa << '[';
    for (int i = 0; i < 6; i++) {
	if (i)
	    sa << ',' << ' ';
	sa << _m[i];
    }
    sa << ']';
    return sa.take_string();
}
