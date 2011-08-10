/*
 * perfcountaccum.{cc,hh} -- accumulate performance counter deltas
 * Eddie Kohler after Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "perfcountaccum.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/perfctr-i586.hh>

PerfCountAccum::PerfCountAccum()
{
}

PerfCountAccum::~PerfCountAccum()
{
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
PerfCountAccum::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String metric_name;
  if (Args(conf, this, errh)
      .read_mp("METRIC", WordArg(), metric_name)
      .complete() < 0)
    return -1;
  _which = PerfCountUser::prepare(metric_name, errh);
  return (_which < 0 ? -1 : 0);
}

int
PerfCountAccum::initialize(ErrorHandler *errh)
{
  _count = _accum = 0;
  return PerfCountUser::initialize(errh);
}

inline void
PerfCountAccum::smaction(Packet *p)
{
  unsigned l, h;
  rdpmc(_which, l, h);
  uint64_t delta =
    PERFCTR_ANNO(p) - (((uint64_t)h << 32) | l);
  _accum += delta;
  _count++;
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
    return String(pca->_count);
   case 1:
    return String(pca->_accum);
   default:
    return String();
  }
}

int
PerfCountAccum::reset_handler(const String &, Element *e, void *, ErrorHandler *)
{
  PerfCountAccum *pca = static_cast<PerfCountAccum *>(e);
  pca->_count = 0;
  pca->_accum = 0;
  return 0;
}

void
PerfCountAccum::add_handlers()
{
  add_read_handler("count", read_handler, 0);
  add_read_handler("accum", read_handler, 1);
  add_write_handler("reset_counts", reset_handler, 0, Handler::BUTTON);
}

ELEMENT_REQUIRES(linuxmodule i586 int64 PerfCountUser)
EXPORT_ELEMENT(PerfCountAccum)
