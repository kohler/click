/*
 * burster.{cc,hh} -- element pulls packets from input, pushes to output
 * in bursts
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
#include "burster.hh"
#include "confparse.hh"
#include "error.hh"

Burster::Burster()
  : _npackets(0), _timer(this)
{
  add_input();
  add_output();
}

Burster::~Burster()
{
}

Burster *
Burster::clone() const
{
  return new Burster;
}

int
Burster::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpMilliseconds, "packet pulling interval", &_interval,
		  cpOptional,
		  cpUnsigned, "max packets per interval", &_npackets,
		  0) < 0)
    return -1;
  if (_npackets <= 0)
    return errh->error("max packets per interval must be > 0");
  return 0;
}

int
Burster::initialize(ErrorHandler *)
{
  _timer.attach(this);
  _timer.schedule_after_ms(_interval);
  return 0;
}

void
Burster::uninitialize()
{
  _timer.unschedule();
}

void
Burster::run_scheduled()
{
  // don't run if the timer is scheduled (an upstream queue went empty but we
  // don't care)
  if (_timer.scheduled())
    return;
  
  for (int i = 0; i < _npackets; i++) {
    Packet *p = input(0).pull();
    if (!p) {
      reschedule();
      click_chatter("burster: warning: rescheduling because no packets left.\n"
		    "                  Used to rely on packet_upstream.\n");
      return;
    }
    output(0).push(p);
  }

  // reset timer
  _timer.schedule_after_ms(_interval);
}

EXPORT_ELEMENT(Burster)
