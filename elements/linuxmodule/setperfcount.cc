/*
 * setperfcount.{cc,hh} -- set performance counter annotations
 * Eddie Kohler after Benjie Chen
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
#include "setperfcount.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
#include <asm/msr.h>

SetPerfCount::SetPerfCount()
{
  add_input();
  add_output();
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
  unsigned l, h;
  rdpmc(_which, l, h);
  unsigned long long hq = h;
  p->set_perfctr_anno((hq << 32) | l);
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
