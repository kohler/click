/*
 * cyclecountaccum.{cc,hh} -- accumulate cycle counter deltas
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
#include "cyclecountaccum.hh"
#include "glue.hh"

CycleCountAccum::CycleCountAccum()
{
  add_input();
  add_output();
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
