/*
 * perfcountaccum.{cc,hh} -- accumulate performance counter deltas
 * Eddie Kohler after Benjie Chen
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
#include "perfcountaccum.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <asm/msr.h>

PerfCountAccum::PerfCountAccum()
{
  add_input();
  add_output();
}

PerfCountAccum *
PerfCountAccum::clone() const
{
  return new PerfCountAccum();
}

void *
PerfCountAccum::cast(const char *n)
{
  if (strcmp(n, "PerfCountUser") == 0)
    return (PerfCountUser *)this;
  else if (strcmp(n, "PerfCountAccum") == 0)
    return (Element *)this;
  else
    return 0;
}

int
PerfCountAccum::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String metric_name;
  if (cp_va_parse(conf, this, errh,
		  cpWord, "performance metric", &metric_name,
		  cpEnd) < 0)
    return -1;
  _which = PerfCountUser::prepare(metric_name, errh);
  return (_which < 0 ? -1 : 0);
}

int
PerfCountAccum::initialize(ErrorHandler *errh)
{
  _npackets = _accum = 0;
  return PerfCountUser::initialize(errh);
}

inline void
PerfCountAccum::smaction(Packet *p)
{
  unsigned l, h;
  rdpmc(_which, l, h);
  unsigned long long delta =
    p->perfctr_anno() - (((unsigned long long)h << 32) | l);
  _accum += delta;
  _npackets++;
}

void
PerfCountAccum::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
PerfCountAccum::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    smaction(p);
  return p;
}

String
PerfCountAccum::read_handler(Element *e, void *thunk)
{
  PerfCountAccum *pca = static_cast<PerfCountAccum *>(e);
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(pca->_npackets) + "\n";
   case 1:
    return String(pca->_accum) + "\n";
   default:
    return String();
  }
}

int
PerfCountAccum::reset_handler(const String &, Element *e, void *, ErrorHandler *)
{
  PerfCountAccum *pca = static_cast<PerfCountAccum *>(e);
  pca->_npackets = 0;
  pca->_accum = 0;
  return 0;
}

void
PerfCountAccum::add_handlers()
{
  add_read_handler("packets", read_handler, (void *)0);
  add_read_handler("accum", read_handler, (void *)1);
  add_write_handler("reset_counts", reset_handler, (void *)0);
}

ELEMENT_REQUIRES(linuxmodule PerfCountUser)
EXPORT_ELEMENT(PerfCountAccum)
