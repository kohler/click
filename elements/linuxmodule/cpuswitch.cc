/*
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
#include "cpuswitch.hh"
#include <click/error.hh>
#include <click/confparse.hh>

CPUSwitch::CPUSwitch()
{
  MOD_INC_USE_COUNT;
  set_ninputs(1);
}

CPUSwitch::~CPUSwitch()
{
  MOD_DEC_USE_COUNT;
}

CPUSwitch *
CPUSwitch::clone() const
{
  return new CPUSwitch;
}

void
CPUSwitch::notify_noutputs(int i)
{
  set_noutputs(i < 1 ? 1 : i);
}

int
CPUSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh, 0) < 0) return -1;
  return 0;
}

void
CPUSwitch::push(int, Packet *p)
{
  int n = current->processor % noutputs();
  output(n).push(p);
}

EXPORT_ELEMENT(CPUSwitch)
ELEMENT_MT_SAFE(CPUSwitch)

