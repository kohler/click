/*
 * staticswitch.{cc,hh} -- element routes packets to one output of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "staticswitch.hh"
#include <click/confparse.hh>

StaticSwitch::StaticSwitch()
{
  MOD_INC_USE_COUNT;
  add_input();
}

StaticSwitch::~StaticSwitch()
{
  MOD_DEC_USE_COUNT;
}

StaticSwitch *
StaticSwitch::clone() const
{
  return new StaticSwitch;
}

void
StaticSwitch::notify_noutputs(int n)
{
  set_noutputs(n);
}

int
StaticSwitch::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _output = 0;
  if (cp_va_parse(conf, this, errh,
		  cpInteger, "active output", &_output,
		  0) < 0)
    return -1;
  if (_output >= noutputs())
    _output = -1;
  return 0;
}

void
StaticSwitch::push(int, Packet *p)
{
  if (_output < 0)
    p->kill();
  else
    output(_output).push(p);
}

EXPORT_ELEMENT(StaticSwitch)
ELEMENT_MT_SAFE(StaticSwitch)
