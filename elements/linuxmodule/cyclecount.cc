/*
 * cyclecount.{cc,hh} -- add cycle counts to element's annotation
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
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
CycleCount::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "index", &_idx,
		     0);
}

inline void
CycleCount::smaction(Packet *p)
{
  p->set_cycle_anno(_idx);
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

EXPORT_ELEMENT(CycleCount)
