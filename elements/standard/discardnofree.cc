/*
 * discardnofree.{cc,hh} -- element pulls packets, doesn't throw them away
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "discardnofree.hh"
#include "scheduleinfo.hh"

DiscardNoFree::DiscardNoFree()
  : Element(1, 0)
{
  MOD_INC_USE_COUNT;
}

DiscardNoFree::~DiscardNoFree()
{
  MOD_DEC_USE_COUNT;
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
  if (input(0).pull())
    reschedule();
}

EXPORT_ELEMENT(DiscardNoFree)
