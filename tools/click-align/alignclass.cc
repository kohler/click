/*
 * alignclass.{cc,hh} -- element classes that know about alignment constraints
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
#include "alignclass.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include "routert.hh"

Alignment
common_alignment(const Vector<Alignment> &a, int off, int n)
{
  if (n == 0)
    return Alignment(1, 0);
  Alignment m = a[off];
  for (int i = 1; i < n; i++)
    m |= a[off+i];
  return m;
}

Alignment
combine_alignment(const Vector<Alignment> &a, int off, int n)
{
  if (n == 0)
    return Alignment(1, 0);
  Alignment m = a[off];
  for (int i = 1; i < n; i++)
    m &= a[off+i];
  return m;
}

Aligner::Aligner()
{
}

void
Aligner::have_flow(const Vector<Alignment> &ain, int offin, int nin, Vector<Alignment> &aout, int offout, int nout)
{
  Alignment a = common_alignment(ain, offin, nin);
  for (int j = 0; j < nout; j++)
    aout[offout + j] = a;
}

void
Aligner::want_flow(Vector<Alignment> &ain, int offin, int nin, const Vector<Alignment> &aout, int offout, int nout)
{
  Alignment a = combine_alignment(aout, offout, nout);
  for (int j = 0; j < nin; j++)
    ain[offin + j] = a;
}

void
CombinedAligner::have_flow(const Vector<Alignment> &ain, int offin, int nin, Vector<Alignment> &aout, int offout, int nout)
{
  _have->have_flow(ain, offin, nin, aout, offout, nout);
}

void
CombinedAligner::want_flow(Vector<Alignment> &ain, int offin, int nin, const Vector<Alignment> &aout, int offout, int nout)
{
  _want->want_flow(ain, offin, nin, aout, offout, nout);
}

void
GeneratorAligner::have_flow(const Vector<Alignment> &, int, int, Vector<Alignment> &aout, int offout, int nout)
{
  for (int j = 0; j < nout; j++)
    aout[offout + j] = _alignment;
}

void
GeneratorAligner::want_flow(Vector<Alignment> &ain, int offin, int nin, const Vector<Alignment> &, int, int)
{
  for (int j = 0; j < nin; j++)
    ain[offin + j] = Alignment();
}

void
ShifterAligner::have_flow(const Vector<Alignment> &ain, int offin, int nin, Vector<Alignment> &aout, int offout, int nout)
{
  Alignment a = common_alignment(ain, offin, nin);
  a += _shift;
  for (int j = 0; j < nout; j++)
    aout[offout + j] = a;
}

void
ShifterAligner::want_flow(Vector<Alignment> &ain, int offin, int nin, const Vector<Alignment> &aout, int offout, int nout)
{
  Alignment a = combine_alignment(aout, offout, nout);
  a -= _shift;
  for (int j = 0; j < nin; j++)
    ain[offin + j] = a;
}

void
WantAligner::want_flow(Vector<Alignment> &ain, int offin, int nin, const Vector<Alignment> &, int, int)
{
  for (int j = 0; j < nin; j++)
    ain[offin + j] = _alignment;
}

void
ClassifierAligner::want_flow(Vector<Alignment> &ain, int offin, int nin, const Vector<Alignment> &aout, int offout, int nout)
{
  Alignment a = combine_alignment(aout, offout, nout);
  if (a.chunk() < 4)
    a = Alignment(4, a.offset());
  for (int j = 0; j < nin; j++)
    ain[offin + j] = a;
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
StripAlignClass::create_aligner(ElementT &e, RouterT *r, ErrorHandler *errh)
{
  int m;
  ContextErrorHandler cerrh(errh, "While analyzing alignment for `" + r->edeclaration(e) + "':");
  if (cp_va_parse(e.configuration, &cerrh,
		  cpInteger, "amount to strip", &m,
		  0) < 0)
    return default_aligner();
  return new ShifterAligner(m);
}


CheckIPHeaderAlignClass::CheckIPHeaderAlignClass(int argno)
  : _argno(argno)
{
}

Aligner *
CheckIPHeaderAlignClass::create_aligner(ElementT &e, RouterT *r, ErrorHandler *errh)
{
  unsigned offset = 0;
  Vector<String> args;
  cp_argvec(e.configuration, args);
  if (args.size() > _argno) {
    if (!cp_unsigned(args[_argno], &offset)) {
      ContextErrorHandler cerrh(errh, "While analyzing alignment for `" + r->edeclaration(e) + "':");
      cerrh.error("argument %d should be IP header offset (unsigned)", _argno + 1);
      return default_aligner();
    }
  }
  return new WantAligner(Alignment(4, 0) - (int)offset);
}


AlignAlignClass::AlignAlignClass()
{
}

Aligner *
AlignAlignClass::create_aligner(ElementT &e, RouterT *r, ErrorHandler *errh)
{
  int offset, chunk;
  ContextErrorHandler cerrh(errh, "While analyzing alignment for `" + r->edeclaration(e) + "':");
  if (cp_va_parse(e.configuration, &cerrh,
		  cpUnsigned, "alignment modulus", &chunk,
		  cpUnsigned, "alignment offset", &offset,
		  0) < 0)
    return default_aligner();
  return new GeneratorAligner(Alignment(chunk, offset));
}
