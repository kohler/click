/*
 * radiosim.{cc,hh} -- simulate 802.11-like radio propagation.
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
#include "radiosim.hh"
#include "confparse.hh"
#include "error.hh"
#include "elements/standard/scheduleinfo.hh"

RadioSim::RadioSim()
{
}

RadioSim *
RadioSim::clone() const
{
  return new RadioSim;
}

int
RadioSim::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return 0;
}

void
RadioSim::notify_noutputs(int n)
{
  set_noutputs(n);
}

void
RadioSim::notify_ninputs(int n)
{
  set_ninputs(n);
}

int
RadioSim::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
RadioSim::uninitialize()
{
  unschedule();
}

void
RadioSim::run_scheduled()
{
  int in, out;

  for(in = 0; in < ninputs(); in++){
    Packet *p = input(in).pull();
    if(p){
      for(out = 0; out < noutputs(); out++){
        output(out).push(p->clone());
      }
      p->kill();
    }
  }

  reschedule();
}

EXPORT_ELEMENT(RadioSim)
