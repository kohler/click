/*
 * discardnofree.{cc,hh} -- element pulls packets, doesn't throw them away
 * Robert Morris
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
#include "discardnofree.hh"
#include "scheduleinfo.hh"

DiscardNoFree::DiscardNoFree()
  : Element(1, 0)
{
}

int
DiscardNoFree::initialize(ErrorHandler *errh)
{
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
DiscardNoFree::uninitialize()
{
  unschedule();
}

void
DiscardNoFree::push(int, Packet *)
{
  // Don't kill().
}

void
DiscardNoFree::run_scheduled()
{
  if (Packet *p = input(0).pull())
    reschedule();
}

EXPORT_ELEMENT(DiscardNoFree)
