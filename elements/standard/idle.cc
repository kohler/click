/*
 * idle.{cc,hh} -- element just sits there and kills packets
 * Robert Morris, Eddie Kohler
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
#include "idle.hh"
#include "bitvector.hh"
#include "scheduleinfo.hh"

Idle::Idle() 
{
  add_output();
}

Idle::~Idle()
{
}

void
Idle::notify_ninputs(int n)
{
  set_ninputs(n);
}

void
Idle::notify_noutputs(int n)
{
  set_noutputs(n);
}

Bitvector
Idle::forward_flow(int) const
{
  return Bitvector(noutputs(), false);
}

Bitvector
Idle::backward_flow(int) const
{
  return Bitvector(ninputs(), false);
}

int
Idle::initialize(ErrorHandler *errh)
{
  for (int i = 0; i < ninputs(); i++)
    if (input_is_pull(i)) {
      ScheduleInfo::join_scheduler(this, errh);
      break;
    }
  return 0;
}

void
Idle::uninitialize()
{
  unschedule();
}

void
Idle::push(int, Packet *p)
{
  p->kill();
}

Packet *
Idle::pull(int)
{
  return 0;
}

void
Idle::run_scheduled()
{
  // XXX reduce tickets if idle
  for (int i = 0; i < ninputs(); i++)
    if (input_is_pull(i))
      if (Packet *p = input(i).pull()) {
	p->kill();
      }
  reschedule();
}

EXPORT_ELEMENT(Idle)
