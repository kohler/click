/*
 * staticswitch.{cc,hh} -- element routes packets to one output of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
