/*
 * alignclass.{cc,hh} -- element classes that know about alignment constraints
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
#include "alignclass.hh"
#include "confparse.hh"

Alignment
common_alignment(const Vector<Alignment> &a, int off, int n)
{
  if (n == 0)
    return Alignment(1, 0);
  Alignment m = a[off];
  for (int i = 1; i < n; i++)
    m *= a[off+i];
  return m;
}

Aligner::Aligner()
{
}

void
Aligner::flow(const Vector<Alignment> &ain, int offin, int nin,
	      Vector<Alignment> &aout, int offout, int nout)
{
  Alignment a = common_alignment(ain, offin, nin);
  for (int j = 0; j < nout; j++)
    aout[offout + j] = a;
}

Alignment
Aligner::want(int)
{
  return Alignment();
}

void
GeneratorAligner::flow(const Vector<Alignment> &, int, int, Vector<Alignment> &aout, int offout, int nout)
{
  for (int j = 0; j < nout; j++)
    aout[offout + j] = _alignment;
}

void
ShifterAligner::flow(const Vector<Alignment> &ain, int offin, int nin, Vector<Alignment> &aout, int offout, int nout)
{
  Alignment a = common_alignment(ain, offin, nin);
  a += _shift;
  for (int j = 0; j < nout; j++)
    aout[offout + j] = a;
}

Alignment
WantAligner::want(int)
{
  return _alignment;
}


Aligner *
default_aligner()
{
  static Aligner *a;
  if (!a) a = new Aligner;
  return a;
}


AlignClass::AlignClass()
  : _aligner(default_aligner())
{
}

AlignClass::AlignClass(Aligner *a)
  : _aligner(a)
{
}

Aligner *
AlignClass::create_aligner(ElementT &, RouterT *, ErrorHandler *)
{
  return _aligner;
}


StripAlignClass::StripAlignClass()
{
}

Aligner *
StripAlignClass::create_aligner(ElementT &e, RouterT *, ErrorHandler *errh)
{
  int m;
  if (cp_va_parse(e.configuration, errh,
		  cpInteger, "amount to strip", &m,
		  0) < 0)
    return default_aligner();
  return new ShifterAligner(m);
}


AlignAlignClass::AlignAlignClass()
{
}

Aligner *
AlignAlignClass::create_aligner(ElementT &e, RouterT *, ErrorHandler *errh)
{
  int offset, chunk;
  if (cp_va_parse(e.configuration, errh,
		  cpUnsigned, "alignment modulus", &chunk,
		  cpUnsigned, "alignment offset", &offset,
		  0) < 0)
    return default_aligner();
  return new GeneratorAligner(Alignment(chunk, offset));
}
