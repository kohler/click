/*
 * cyclecountaccum.{cc,hh} -- accumulate cycle counter deltas
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

#include <click/config.h>
#include <click/package.hh>
#include "cyclecountaccum.hh"
#include <click/glue.hh>

CycleCountAccum::CycleCountAccum()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

CycleCountAccum::~CycleCountAccum()
{
  MOD_DEC_USE_COUNT;
}

CycleCountAccum *
CycleCountAccum::clone() const
{
  return new CycleCountAccum;
}

int
CycleCountAccum::initialize(ErrorHandler *)
{
  _npackets = _accum = 0;
  return 0;
}

inline void
CycleCountAccum::smaction(Packet *p)
{
  _accum += click_get_cycles() - p->perfctr_anno();
  _npackets++;
}

void
CycleCountAccum::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
CycleCountAccum::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    smaction(p);
  return p;
}

String
CycleCountAccum::read_handler(Element *e, void *thunk)
{
  CycleCountAccum *cca = static_cast<CycleCountAccum *>(e);
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(cca->_npackets) + "\n";
   case 1:
    return String(cca->_accum) + "\n";
   default:
    return String();
  }
}

int
CycleCountAccum::reset_handler(const String &, Element *e, void *, ErrorHandler *)
{
  CycleCountAccum *cca = static_cast<CycleCountAccum *>(e);
  cca->_npackets = 0;
  cca->_accum = 0;
  return 0;
}

void
CycleCountAccum::add_handlers()
{
  add_read_handler("packets", read_handler, (void *)0);
  add_read_handler("cycles", read_handler, (void *)1);
  add_write_handler("reset_counts", reset_handler, (void *)0);
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(CycleCountAccum)
