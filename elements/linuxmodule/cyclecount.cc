/*
 * cyclecount.{cc,hh} -- add cycle counts to element's annotation
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
#include "cyclecount.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

CycleCount::CycleCount()
{
  add_input();
  add_output();
}

CycleCount::~CycleCount()
{
}

CycleCount *
CycleCount::clone() const
{
  return new CycleCount();
}

int
CycleCount::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "index", &_idx,
		     0);
}

#if 0
__inline__ unsigned long long
click_get_cycles(void)
{
    unsigned long low, high;
    unsigned long long x;

    __asm__ __volatile__("rdtsc":"=a" (low), "=d" (high));
    x = high;
    x <<= 32;
    x |= low;
    return(x);
}
#endif

inline void
CycleCount::smaction(Packet *p)
{
  p->set_cycle_anno(_idx, get_cycles());
}

void
CycleCount::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
CycleCount::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    smaction(p);
  return(p);
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(CycleCount)
