/*
 * cyclecount.{cc,hh} -- add cycle counts to element's annotation
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
#include "cyclecount.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

CycleCount::CycleCount()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

CycleCount::~CycleCount()
{
  MOD_DEC_USE_COUNT;
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

ELEMENT_REQUIRES(linuxmodule false)
EXPORT_ELEMENT(CycleCount)
