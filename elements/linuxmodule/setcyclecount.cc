/*
 * setcyclecount.{cc,hh} -- set cycle counter annotation
 * Eddie Kohler
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
#include "setcyclecount.hh"
#include <click/glue.hh>

SetCycleCount::SetCycleCount()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SetCycleCount::~SetCycleCount()
{
  MOD_DEC_USE_COUNT;
}

SetCycleCount *
SetCycleCount::clone() const
{
  return new SetCycleCount();
}

void
SetCycleCount::push(int, Packet *p)
{
  p->set_perfctr_anno(click_get_cycles());
  output(0).push(p);
}

Packet *
SetCycleCount::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    p->set_perfctr_anno(click_get_cycles());
  return p;
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(SetCycleCount)
