/*
 * burster.{cc,hh} -- element pulls packets from input, pushes to output
 * in bursts
 * Eddie Kohler
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

#include <click/config.h>
#include "burster.hh"
#include <click/confparse.hh>
#include <click/error.hh>

Burster::Burster()
  : Element(1, 1), _npackets(0), _timer(this)
{
  MOD_INC_USE_COUNT;
}

Burster::~Burster()
{
  MOD_DEC_USE_COUNT;
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
  _timer.initialize(this);
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
    if (!p)
      break;
    output(0).push(p);
  }

  // reset timer
  _timer.reschedule_after_ms(_interval);
}

EXPORT_ELEMENT(Burster)
