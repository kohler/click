/*
 * setcyclecount.{cc,hh} -- set cycle counter annotation
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
#include "setcyclecount.hh"
#include "glue.hh"

SetCycleCount::SetCycleCount()
{
  add_input();
  add_output();
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
