/*
 * setperfcount.{cc,hh} -- set performance counter annotations
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
#include "setperfcount.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/perfctr-i586.hh>

SetPerfCount::SetPerfCount()
{
}

SetPerfCount::~SetPerfCount()
{
}

void *
SetPerfCount::cast(const char *n)
{
  if (strcmp(n, "PerfCountUser") == 0)
    return (PerfCountUser *)this;
  else if (strcmp(n, "SetPerfCount") == 0)
    return (Element *)this;
  else
    return 0;
}

int
SetPerfCount::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String metric_name;
    if (Args(conf, this, errh)
	.read_mp("METRIC", WordArg(), metric_name)
	.complete() < 0)
	return -1;
    _which = PerfCountUser::prepare(metric_name, errh);
    return (_which < 0 ? -1 : 0);
}

inline void
SetPerfCount::smaction(Packet *p)
{
  unsigned l, h;
  rdpmc(_which, l, h);
  uint64_t hq = h;
  SET_PERFCTR_ANNO(p, (hq << 32) | l);
}

void
SetPerfCount::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
SetPerfCount::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    smaction(p);
  return p;
}

ELEMENT_REQUIRES(linuxmodule i586 int64 PerfCountUser)
EXPORT_ELEMENT(SetPerfCount)
