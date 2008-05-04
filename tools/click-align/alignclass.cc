/*
 * alignclass.{cc,hh} -- element classes that know about alignment constraints
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2007 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include "alignclass.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/bitvector.hh>
#include "processingt.hh"
#include "routert.hh"
#include <string.h>

Alignment
common_alignment(Vector<Alignment>::const_iterator a, int n)
{
  if (n == 0)
    return Alignment(1, 0);
  Alignment m = *a;
  for (int i = 1; i < n; i++)
      m |= *++a;
  return m;
}

Alignment
combine_alignment(Vector<Alignment>::const_iterator a, int n)
{
  if (n == 0)
    return Alignment(1, 0);
  Alignment m = *a;
  for (int i = 1; i < n; i++)
      m &= *++a;
  return m;
}

void
Aligner::have_flow(Vector<Alignment>::const_iterator ain, int nin, Vector<Alignment>::iterator aout, int nout, const String &flow_code)
{
    Bitvector b;
    for (int j = 0; j < nout; ++j, ++aout) {
	ProcessingT::backward_flow(flow_code, j, &b, nin);
	Alignment a;
	for (int i = 0; i < nin; ++i)
	    if (b[i])
		a |= ain[i];
	*aout = a;
    }
    //Alignment a = common_alignment(ain, nin);
    //for (int j = 0; j < nout; ++j, ++aout)
    //    *aout = a;
}

void
Aligner::want_flow(Vector<Alignment>::iterator ain, int nin, Vector<Alignment>::const_iterator aout, int nout, const String &flow_code)
{
    Bitvector b;
    for (int i = 0; i < nin; ++i, ++ain) {
	ProcessingT::forward_flow(flow_code, i, &b, nout);
	Alignment a;
	for (int j = 0; j < nout; ++j)
	    if (b[j])
		a &= aout[j];
	*ain = a;
    }

    //Alignment a = combine_alignment(aout, nout);
    //for (int j = 0; j < nin; ++j, ++ain)
    //   *ain = a;
}

void
Aligner::adjust_flow(Vector<Alignment>::iterator, int, Vector<Alignment>::const_iterator, int)
{
}

void
NullAligner::have_flow(Vector<Alignment>::const_iterator, int, Vector<Alignment>::iterator, int, const String &)
{
}

void
NullAligner::want_flow(Vector<Alignment>::iterator, int, Vector<Alignment>::const_iterator, int, const String &)
{
}

void
CombinedAligner::have_flow(Vector<Alignment>::const_iterator ain, int nin, Vector<Alignment>::iterator aout, int nout, const String &flow_code)
{
    _have->have_flow(ain, nin, aout, nout, flow_code);
}

void
CombinedAligner::want_flow(Vector<Alignment>::iterator ain, int nin, Vector<Alignment>::const_iterator aout, int nout, const String &flow_code)
{
    _want->want_flow(ain, nin, aout, nout, flow_code);
}

void
GeneratorAligner::have_flow(Vector<Alignment>::const_iterator, int, Vector<Alignment>::iterator aout, int nout, const String &)
{
    for (int j = 0; j < nout; ++j, ++aout)
	*aout = _alignment;
}

void
GeneratorAligner::want_flow(Vector<Alignment>::iterator ain, int nin, Vector<Alignment>::const_iterator, int, const String &)
{
    for (int j = 0; j < nin; ++j, ++ain)
	*ain = Alignment();
}

void
ShifterAligner::have_flow(Vector<Alignment>::const_iterator ain, int nin, Vector<Alignment>::iterator aout, int nout, const String &)
{
  Alignment a = common_alignment(ain, nin);
  a += _shift;
  for (int j = 0; j < nout; ++j, ++aout)
      *aout = a;
}

void
ShifterAligner::want_flow(Vector<Alignment>::iterator ain, int nin, Vector<Alignment>::const_iterator aout, int nout, const String &)
{
  Alignment a = combine_alignment(aout, nout);
  a -= _shift;
  for (int j = 0; j < nin; ++j, ++ain)
      *ain = a;
}

void
WantAligner::want_flow(Vector<Alignment>::iterator ain, int nin, Vector<Alignment>::const_iterator, int, const String &)
{
    for (int j = 0; j < nin; ++j, ++ain)
	*ain = _alignment;
}

void
ClassifierAligner::adjust_flow(Vector<Alignment>::iterator ain, int nin, Vector<Alignment>::const_iterator, int)
{
    Alignment a = common_alignment(ain, nin);
    if (a.chunk() < 4)
	a = Alignment(4, a.offset());
    for (int j = 0; j < nin; ++j, ++ain)
	*ain = a;
}


Aligner *
default_aligner()
{
  static Aligner *a;
  if (!a) a = new Aligner;
  return a;
}


AlignClass::AlignClass(const String &name)
  : ElementClassT(name), _aligner(default_aligner())
{
}

AlignClass::AlignClass(const String &name, Aligner *a)
  : ElementClassT(name), _aligner(a)
{
}

Aligner *
AlignClass::create_aligner(ElementT *, RouterT *, ErrorHandler *)
{
  return _aligner;
}

void *
AlignClass::cast(const char *s)
{
  if (strcmp(s, "AlignClass") == 0)
    return (void *)this;
  else
    return 0;
}


StripAlignClass::StripAlignClass(const String &name, bool is_strip)
    : AlignClass(name), _is_strip(is_strip)
{
}

Aligner *
StripAlignClass::create_aligner(ElementT *e, RouterT *, ErrorHandler *errh)
{
    Vector<String> args;
    cp_argvec(e->configuration(), args);
    unsigned nbytes;
    ContextErrorHandler cerrh(errh, "While analyzing alignment for '" + e->declaration() + "':");
    if (cp_va_kparse(args, &cerrh,
		     "LENGTH", cpkP+cpkM, cpUnsigned, &nbytes,
		     cpEnd) < 0)
	return default_aligner();
    return new ShifterAligner(_is_strip ? nbytes : -nbytes);
}


CheckIPHeaderAlignClass::CheckIPHeaderAlignClass(const String &name)
    : AlignClass(name)
{
}

Aligner *
CheckIPHeaderAlignClass::create_aligner(ElementT *e, RouterT *, ErrorHandler *)
{
    Vector<String> args;
    cp_argvec(e->configuration(), args);
    unsigned offset = 0;
    // Old CheckIPHeader elements might have a BADSRC* argument before the
    // OFFSET argument.  This magic supposedly parses either.
    if (cp_va_kparse(args, ErrorHandler::silent_handler(), "OFFSET", cpkP, cpUnsigned, &offset, cpIgnoreRest, cpEnd) < 0)
	(void) cp_va_kparse(args, ErrorHandler::silent_handler(), "BADSRC*", cpkP, cpIgnore, "OFFSET", cpkP, cpUnsigned, &offset, cpIgnoreRest, cpEnd);
    return new WantAligner(Alignment(4, 0) - (int)offset);
}


AlignAlignClass::AlignAlignClass()
  : AlignClass("Align")
{
}

Aligner *
AlignAlignClass::create_aligner(ElementT *e, RouterT *, ErrorHandler *errh)
{
  int offset, chunk;
  ContextErrorHandler cerrh(errh, "While analyzing alignment for '" + e->declaration() + "':");
  if (cp_va_kparse(e->configuration(), &cerrh,
		   "MODULUS", cpkP+cpkM, cpUnsigned, &chunk,
		   "OFFSET", cpkP+cpkM, cpUnsigned, &offset,
		   cpEnd) < 0)
    return default_aligner();
  return new GeneratorAligner(Alignment(chunk, offset));
}
