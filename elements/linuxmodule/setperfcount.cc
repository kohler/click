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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#if __i386__
#include <asm/msr.h>
#endif

SetPerfCount::SetPerfCount()
{
  // no MOD_INC_USE_COUNT; rely on PerfCountUser
  add_input();
  add_output();
}

SetPerfCount::~SetPerfCount()
{
  // no MOD_DEC_USE_COUNT; rely on PerfCountUser
}

SetPerfCount *
SetPerfCount::clone() const
{
  return new SetPerfCount();
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
SetPerfCount::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String metric_name;
  if (cp_va_parse(conf, this, errh,
		  cpWord, "performance metric", &metric_name,
		  cpEnd) < 0)
    return -1;
  _which = PerfCountUser::prepare(metric_name, errh);
  return (_which < 0 ? -1 : 0);
}

inline void
SetPerfCount::smaction(Packet *p)
{
#if __i386__
  unsigned l, h;
  rdpmc(_which, l, h);
  unsigned long long hq = h;
  p->set_perfctr_anno((hq << 32) | l);
#else
  // code for other architectures
#endif
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

ELEMENT_REQUIRES(linuxmodule PerfCountUser)
EXPORT_ELEMENT(SetPerfCount)
