/*
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
CPUSwitch::configure(const Vector<String> &conf, ErrorHandler *errh)
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

